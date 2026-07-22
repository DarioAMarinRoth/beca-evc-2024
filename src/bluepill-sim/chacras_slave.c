#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <sys/select.h>
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

/* Corriente simulada por motor: proporcional a la velocidad real */
#define CORRIENTE_POR_RPS   0.05f   /* A por RPS */
#define VBAT_NOMINAL        24.0f   /* V */

/* Dinámica de primer orden: cuanto más chico, más rápido responde el motor */
#define TAU_SECONDS         0.2f    /* s */
#define SIM_STEP_USEC       50000   /* 50 ms -> paso de simulación */

static modbus_t *ctx = NULL;
static modbus_mapping_t *mb_mapping = NULL;
static volatile int running = 1;

/* ---------- conversión float <-> 2 registros de 16 bits ---------- */

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

/* ---------- simulación de dinámica ---------- */

/*
 * Avanza un paso de simulación de dt segundos.
 * La velocidad real de cada motor persigue el setpoint con una respuesta
 * de primer orden, PERO solo si el sistema está armado. Si está desarmado,
 * el objetivo pasa a ser 0, replicando que el firmware real ignora los
 * setpoints mientras el bit de armado está en 0.
 */
static void simulate_step(float dt) {
    int armado = mb_mapping->tab_registers[ARMADO_BASE];
    float ibat_total = 0.0f;

    for (int m = 0; m < NUM_MOTORS; m++) {
        int base = MOTOR_BASE + m * 6;

        uint16_t setpoint = mb_mapping->tab_registers[base + 0];
        float target = armado ? (float)setpoint : 0.0f;

        float vel = regs_to_float(mb_mapping->tab_registers[base + 2],
                                   mb_mapping->tab_registers[base + 3]);

        /* respuesta de primer orden hacia el target */
        vel += (target - vel) * (dt / TAU_SECONDS);

        /* evitar residuos numéricos infinitesimales cuando ya llegó */
        if (fabsf(vel - target) < 0.01f) {
            vel = target;
        }

        float corriente = fabsf(vel) * CORRIENTE_POR_RPS;

        float_to_regs(vel,
                      &mb_mapping->tab_registers[base + 2],
                      &mb_mapping->tab_registers[base + 3]);
        float_to_regs(corriente,
                      &mb_mapping->tab_registers[base + 4],
                      &mb_mapping->tab_registers[base + 5]);

        ibat_total += corriente;
    }

    float_to_regs(ibat_total,
                  &mb_mapping->tab_registers[STATUS_BASE + 0],
                  &mb_mapping->tab_registers[STATUS_BASE + 1]);

    float_to_regs(VBAT_NOMINAL,
                  &mb_mapping->tab_registers[STATUS_BASE + 2],
                  &mb_mapping->tab_registers[STATUS_BASE + 3]);
}

/* ---------- persistencia ---------- */

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
        /* aun sin archivo, inicializar los registros derivados (vbat, etc.) */
        simulate_step(0.0f);
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
        simulate_step(0.0f);
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

    /*
     * Nota: al recuperar un setpoint > 0 desde el archivo, la velocidad
     * real arranca en 0 y vuelve a converger con la rampa normal en vez
     * de "saltar" directamente al setpoint. Es el comportamiento correcto:
     * el motor real tampoco arranca ya girando al setpoint guardado.
     */
    simulate_step(0.0f);

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

    /* cargar estado persistido (o inicializar derivados) antes de conectar */
    load_state();

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "modbus_connect: %s\n", modbus_strerror(errno));
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        return EXIT_FAILURE;
    }

    printf("[chacras_slave] Escuchando en %s @ %d bps  (Ctrl+C para salir)\n",
           serialport, baudrate);
    printf("[chacras_slave] Simulando dinamica de motores con TAU=%.2fs, paso=%dms\n",
           TAU_SECONDS, SIM_STEP_USEC / 1000);

    uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];
    int fd = modbus_get_socket(ctx);
    float dt_seconds = SIM_STEP_USEC / 1000000.0f;

    while (running) {
        fd_set rfds;
        struct timeval tv;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = SIM_STEP_USEC;

        int sel = select(fd + 1, &rfds, NULL, NULL, &tv);

        if (sel == -1) {
            if (errno == EINTR) break;   /* señal recibida */
            continue;
        }

        if (sel == 0) {
            /* timeout: no llegó ninguna trama en este intervalo,
               avanzar un paso de la simulación de dinámica */
            simulate_step(dt_seconds);
            continue;
        }

        /* hay una trama Modbus esperando */
        int rc = modbus_receive(ctx, query);
        if (rc == -1) {
            if (errno == EINTR) break;
            continue;
        }

        /* guardar snapshot de registros ANTES de responder, para detectar
           qué escribió el master (setpoint, sentido, armado) */
        uint16_t regs_before[NUM_REGS];
        memcpy(regs_before, mb_mapping->tab_registers, sizeof(regs_before));

        modbus_reply(ctx, query, rc, mb_mapping);

        int changed = 0;
        for (int i = 0; i < NUM_REGS; i++) {
            if (mb_mapping->tab_registers[i] != regs_before[i]) {
                printf("[write] reg[%d]: %u -> %u\n",
                       i, regs_before[i], mb_mapping->tab_registers[i]);
                changed = 1;
            }
        }

        if (changed) {
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