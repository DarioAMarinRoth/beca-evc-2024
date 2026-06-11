#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <modbus/modbus-rtu.h>
#include <getopt.h>

// https://libmodbus.org/
// gcc -o master chacras.c -lmodbus

/*************************************
Dirección   Función

0           Setpoint (Motor 1)
1           Sentido (Motor 1)
2           Velocidad (Motor 1) H
3           Velocidad (Motor 1) L
4           Corriente (Motor 1) H
5           Corriente (Motor 1) L
6           Setpoint (Motor 2)
7           Sentido (Motor 2)
8           Velocidad (Motor 2) H
9           Velocidad (Motor 2) L
10          Corriente (Motor 2) H
11          Corriente (Motor 2) L
12          Setpoint (Motor 3)
13          Sentido (Motor 3)
14          Velocidad (Motor 3) H
15          Velocidad (Motor 3) L
16          Corriente (Motor 3) H
17          Corriente (Motor 3) L
18          Setpoint (Motor 4)
19          Sentido (Motor 4)
20          Velocidad (Motor 4) H
21          Velocidad (Motor 4) L
22          Corriente (Motor 4) H
23          Corriente (Motor 4) L
24          Reservado
25          Reservado
26          Tensión de Baterías H
27          Tensión de Baterías L          
28          Armado de sistema
29          Reservado
30          Reservado
31          Reservado
*************************************/

#define CHACRAS_ADDR    1
#define MOTOR_BASE      0
#define STATUS_BASE     24
#define ARMADO_BASE     28

typedef struct __attribute__((packed)) MotorStruct {
    uint16_t setpoint;
    uint16_t sentido;
    float velocidad;
    float corriente;
} MotorStruct_t;

void usage(void);
modbus_t * modbus_init(char * serialport, int baudrate);
int modbus_wr(modbus_t *ctx, int addr, uint16_t * data, int count);
int modbus_rd(modbus_t *ctx, int addr, uint16_t * data, int count);

int main(int argc, char *argv[]) {
    char serialport[16] = "/dev/ttyV0"; // Originalmente tenía el usb0
    int baudrate = 115200;
    modbus_t *ctx = NULL;
    int serialport_opt = 0;
    int baudrate_opt = 0;
    int status_opt = 0;
    int motor_opt = 0;
    int armado_opt = 0;
    int n_motor;
    int nargs = 0;

    if (argc==1) {
        usage();
        exit(EXIT_SUCCESS);
    }

    /* parse options */
    int option_index = 0, opt;
    static struct option loptions[] = {
        {"help",       no_argument,       0, 'h'},
        {"port",       required_argument, 0, 'p'},
        {"baud",       required_argument, 0, 'b'},
        {"status",     no_argument,       0, 's'},
        {"motor",      required_argument, 0, 'm'},
        {"armado",     required_argument, 0, 'a'}
    };

    while(1) {
        opt = getopt_long (argc, argv, "hp:b:m:a:s",
                           loptions, &option_index);
        if (opt==-1) break;
        switch (opt) {
        case 'h':
            if (argc != 2) {
                usage();

                exit(EXIT_FAILURE);
            }
            usage();
            break;
        case 'b':
            baudrate = strtol(optarg, NULL, 10);
            baudrate_opt = 1;
            nargs += 2;
            break;
        case 'p':
            strcpy(serialport,optarg);
            serialport_opt = 1;
            nargs += 2;
            break;
        case 's':
            status_opt = 1;
            break;
        case 'm':
            motor_opt = 1;
            break;
        case 'a':
            armado_opt = 1;
            break;
        }
    }

    ctx = modbus_init(serialport, baudrate);
    if (!ctx) {
        exit(EXIT_FAILURE);
    }

    if ((status_opt && motor_opt && armado_opt) ||
        (status_opt && motor_opt) ||
        (status_opt && armado_opt) ||
        (motor_opt && armado_opt)) {

        usage();

        exit(EXIT_FAILURE);
    }

    if (status_opt) {
        printf("Status\n");
        uint16_t status[5];
        modbus_rd(ctx, STATUS_BASE, status, 5);
        float ibat = *(float *)&status[0];
        float vbat = *(float *)&status[2];
        int armado = status[4];
        printf("%f %f %d\n", ibat, vbat, armado);
    }

    if (motor_opt) {
        if ((argc == 3 && !serialport_opt && !baudrate_opt) ||
            (argc == 5 && serialport_opt && !baudrate_opt) ||
            (argc == 5 && !serialport_opt && baudrate_opt) ||
            (argc == 7 && serialport_opt && baudrate_opt)) {

            n_motor = atoi(argv[nargs + 2]);
            printf("Motor %d\n", n_motor);
            uint16_t motor[6];
            if ((n_motor < 0) || (n_motor > 3)) {
                usage();

                exit(EXIT_FAILURE);
            }
            modbus_rd(ctx, MOTOR_BASE+n_motor*6, motor, 6);
            MotorStruct_t *pt = (MotorStruct_t *)motor;
            printf("%d %d %f %f\n", pt->setpoint,   //
                                    pt->sentido,    // 
                                    pt->velocidad,  //
                                    pt->corriente);
        } else if ((argc == 5 && !serialport_opt && !baudrate_opt) ||
                   (argc == 7 && serialport_opt && !baudrate_opt) ||
                   (argc == 7 && !serialport_opt && baudrate_opt) ||
                   (argc == 9 && serialport_opt && baudrate_opt)) {
            uint16_t data[2];
            n_motor = atoi(argv[nargs + 2]);
            data[0] = atoi(argv[nargs + 3]);
            data[1] = atoi(argv[nargs + 4]);
            if ((n_motor < 0) || (n_motor > 3)) {
                usage();

                exit(EXIT_FAILURE);
            }
            printf("Motor %d RPS %d Sentido: %d\n", n_motor, data[0], data[1]);
            modbus_wr(ctx, MOTOR_BASE + 6*n_motor, data, 2);
        } else {
            usage();

            exit(EXIT_FAILURE);
        }
    }

    if (armado_opt) {
        printf("argc: %d serialport_opt: %d baudrate_opt: %d nargs: %d\n", argc, serialport_opt, baudrate_opt, nargs);
        if ((argc == 3 && !serialport_opt && !baudrate_opt) ||
            (argc == 5 && serialport_opt && !baudrate_opt) ||
            (argc == 5 && !serialport_opt && baudrate_opt) ||
            (argc == 7 && serialport_opt && baudrate_opt)) {  
                int val = atoi(argv[nargs + 2]);
                printf("Armado %d\n", val);
                modbus_wr(ctx, ARMADO_BASE, (uint16_t *)&val, 1);
        } else {
            usage();

            exit(EXIT_FAILURE);
        }
    }

    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
    }

    return 0;
}

modbus_t * modbus_init(char * serialport, int baudrate) {
    modbus_t *ctx;

    // Create a new RTU context with proper serial parameters
    ctx = modbus_new_rtu(serialport, baudrate, 'N', 8, 1);
    if (!ctx) {
        fprintf(stderr, "Failed to create the context: %s\n", modbus_strerror(errno));
    }

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Unable to connect: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        ctx = NULL;
    }

    // Set the Modbus address of the remote slave
    modbus_set_slave(ctx, CHACRAS_ADDR);

    return ctx;
}

int modbus_wr(modbus_t *ctx, int addr, uint16_t * data, int count) {
    int res = 1;

    // Write multiple registers
    int rc = modbus_write_registers(ctx, addr, count, data); 
    if (rc == -1) {
        fprintf(stderr, "Failed to write multiple registers: %s\n", modbus_strerror(errno));
        res = 0;
    } else {
        printf("Successfully wrote %d registers starting from address %d\n", rc, addr);
        res = 0;
    }

    return res;
}

int modbus_rd(modbus_t *ctx, int addr, uint16_t * data, int count) {
    int num;
    num = modbus_read_registers(ctx, addr, count, data);
    if (num != count) {
        fprintf(stderr, "Failed to read: %s\n", modbus_strerror(errno));
        num = 0;
    }

    return num;
}

void usage(void) {
    printf("Usage: chacras [OPTIONS]\n"
    "\n"
    "Options:\n"
    "  -h, --help                       Print this help message\n"
    "  -p, --port=serialport            Serial port\n"
    "  -b, --baud=baudrate              Baudrate (bps)\n"
    "  -s, --status                     Tension bateria, corriente total, armado\n"
    "  -m, --motor n                    Muestra el estado del motor n\n"
    "  -m, --motor n setpoint sentido   Establece setpoint y sentido del motor n\n"
    "  -a, --armado n                   Armado del sistema n = 1\n"
   "\n"
    );
}

