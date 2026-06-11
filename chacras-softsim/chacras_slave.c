#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <modbus/modbus-rtu.h>
#include <cjson/cJSON.h>

// gcc -o chacras_slave chacras_slave.c -lmodbus -lcjson -lm

/*************************************
Mapa de registros (igual que la blue pill):

0-5    Motor 1: setpoint, sentido, vel_H, vel_L, corr_H, corr_L
6-11   Motor 2
12-17  Motor 3
18-23  Motor 4
24-25  Corriente batería H/L  (float)
26-27  Tensión batería H/L    (float)
28     Armado
29-31  Reservado
*************************************/

#define NUM_REGS        32
#define CHACRAS_ADDR    1
#define MOTOR_BASE      0
#define STATUS_BASE     24
#define ARMADO_BASE     28
#define NUM_MOTORS      4
#define PERSIST_FILE    "chacras_state.json"

/* Corriente simulada por motor: proporcional al setpoint */
#define CORRIENTE_POR_RPS   0.05f   /* A por RPS */
#define VBAT_NOMINAL        24.0f   /* V */

static uint16_t regs[NUM_REGS];
static modbus_t *ctx = NULL;
static modbus_mapping_t *mb_mapping = NULL;
static volatile int running = 1;

/* ---------- persistencia ---------- */

static void float_to_regs(float val, uint16_t *hi, uint16_t *lo) {
    uint32_t raw;
    memcpy(&raw, &val, sizeof(float));
    *hi = (raw >> 16) & 0xFFFF;
    *lo =  raw        & 0xFFFF;
}

static float regs_to_float(uint16_t hi, uint16_t lo) {
    uint32_t raw = ((uint32_t)hi << 16) | lo;
    float val;
    memcpy(&val, &raw, sizeof(float));
    return val;
}

static void recalc_corrientes(void) {
    for (int m = 0; m < NUM_MOTORS; m++) {
        int base = MOTOR_BASE + m * 6;
        uint16_t sp = mb_mapping->tab_registers[base + 0];
        float corriente = sp * CORRIENTE_POR_RPS;
        /* velocidad = setpoint (simplificado: sin rampa) */
        float_to_regs((float)sp,
                      &mb_mapping->tab_registers[base + 2],
                      &mb_mapping->tab_registers[base + 3]);
        float_to_regs(corriente,
                      &mb_mapping->tab_registers[base + 4],
                      &mb_mapping->tab_registers[base + 5]);
    }

    /* corriente total de batería = suma de corrientes de motores */
    float ibat = 0.0f;
    for (int m = 0; m < NUM_MOTORS; m++) {
        int base = MOTOR_BASE + m * 6;
        ibat += regs_to_float(mb_mapping->tab_registers[base + 4],
                              mb_mapping->tab_registers[base + 5]);
    }
    float_to_regs(ibat,
                  &mb_mapping->tab_registers[STATUS_BASE + 0],
                  &mb_mapping->tab_registers[STATUS_BASE + 1]);

    /* tensión de batería: fija nominal */
    float_to_regs(VBAT_NOMINAL,
                  &mb_mapping->tab_registers[STATUS_BASE + 2],
                  &mb_mapping->tab_registers[STATUS_BASE + 3]);
}

static void save_state(void) {
    cJSON *root = cJSON_CreateObject();

    cJSON *motors = cJSON_AddArrayToObject(root, "motors");
    for (int m = 0; m < NUM_MOTORS; m++) {
        int base = MOTOR_BASE + m * 6;
        cJSON *motor = cJSON_CreateObject();
        cJSON_AddNumberToObject(motor, "setpoint", mb_mapping->tab_registers[base + 0]);
        cJSON_AddNumberToObject(motor, "sentido",  mb_mapping->tab_registers[base + 1]);
        cJSON_AddItemToArray(motors, motor);
    }

    cJSON_AddNumberToObject(root, "armado", mb_mapping->tab_registers[ARMADO_BASE]);

    char *json_str = cJSON_PrintUnformatted(root);
    FILE *f = fopen(PERSIST_FILE, "w");
    if (f) {
        fputs(json_str, f);
        fclose(f);
        printf("[persist] Estado guardado en %s\n", PERSIST_FILE);
    } else {
        fprintf(stderr, "[persist] No se pudo guardar: %s\n", strerror(errno));
    }
    free(json_str);
    cJSON_Delete(root);
}

static void load_state(void) {
    FILE *f = fopen(PERSIST_FILE, "r");
    if (!f) {
        printf("[persist] No existe %s, arrancando con valores por defecto\n", PERSIST_FILE);
        return;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        fprintf(stderr, "[persist] JSON inválido, arrancando desde cero\n");
        return;
    }

    cJSON *motors = cJSON_GetObjectItem(root, "motors");
    if (motors) {
        int n = cJSON_GetArraySize(motors);
        if (n > NUM_MOTORS) n = NUM_MOTORS;
        for (int m = 0; m < n; m++) {
            int base = MOTOR_BASE + m * 6;
            cJSON *motor = cJSON_GetArrayItem(motors, m);
            cJSON *sp  = cJSON_GetObjectItem(motor, "setpoint");
            cJSON *sen = cJSON_GetObjectItem(motor, "sentido");
            if (sp)  mb_mapping->tab_registers[base + 0] = (uint16_t)sp->valuedouble;
            if (sen) mb_mapping->tab_registers[base + 1] = (uint16_t)sen->valuedouble;
        }
    }

    cJSON *armado = cJSON_GetObjectItem(root, "armado");
    if (armado) mb_mapping->tab_registers[ARMADO_BASE] = (uint16_t)armado->valuedouble;

    cJSON_Delete(root);

    /* recalcular los registros derivados con los valores cargados */
    recalc_corrientes();

    printf("[persist] Estado cargado desde %s\n", PERSIST_FILE);
}

/* ---------- señales ---------- */

static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

/* ---------- main ---------- */

int main(int argc, char *argv[]) {
    char serialport[64] = "/dev/ttyV1";
    int baudrate = 115200;

    if (argc >= 2) strncpy(serialport, argv[1], sizeof(serialport) - 1);
    if (argc >= 3) baudrate = atoi(argv[2]);

    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    /* contexto modbus */
    ctx = modbus_new_rtu(serialport, baudrate, 'N', 8, 1);
    if (!ctx) {
        fprintf(stderr, "modbus_new_rtu: %s\n", modbus_strerror(errno));
        return EXIT_FAILURE;
    }

    modbus_set_slave(ctx, CHACRAS_ADDR);

    /* DEBUG: descomentar para ver tramas crudas */
    /* modbus_set_debug(ctx, TRUE); */

    mb_mapping = modbus_mapping_new(0, 0, NUM_REGS, 0);
    if (!mb_mapping) {
        fprintf(stderr, "modbus_mapping_new: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return EXIT_FAILURE;
    }

    /* cargar estado persistido antes de conectar */
    load_state();

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "modbus_connect: %s\n", modbus_strerror(errno));
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        return EXIT_FAILURE;
    }

    printf("[chacras_slave] Escuchando en %s @ %d bps  (Ctrl+C para salir)\n",
           serialport, baudrate);

    uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];

    while (running) {
        int rc = modbus_receive(ctx, query);
        if (rc == -1) {
            if (errno == EINTR) break;      /* señal recibida */
            /* timeout u otro error no fatal, seguir */
            continue;
        }

        /* guardar snapshot de registros escritos ANTES de responder */
        uint16_t regs_before[NUM_REGS];
        memcpy(regs_before, mb_mapping->tab_registers, sizeof(regs_before));

        modbus_reply(ctx, query, rc, mb_mapping);

        /* detectar escrituras: comparar registros */
        int changed = 0;
        for (int i = 0; i < NUM_REGS; i++) {
            if (mb_mapping->tab_registers[i] != regs_before[i]) {
                printf("[write] reg[%d]: %u -> %u\n",
                       i, regs_before[i], mb_mapping->tab_registers[i]);
                changed = 1;
            }
        }

        if (changed) {
            recalc_corrientes();
            save_state();
        }
    }

    printf("\n[chacras_slave] Cerrando...\n");
    save_state();
    modbus_close(ctx);
    modbus_mapping_free(mb_mapping);
    modbus_free(ctx);

    return EXIT_SUCCESS;
}
