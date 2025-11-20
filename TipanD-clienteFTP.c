/* Proyecto - Cliente FTP Concurrente */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h> 
#include <sys/wait.h> 
#include <pwd.h>

extern int  errno;

int  errexit(const char *format, ...);
int  connectTCP(const char *host, const char *service);
int  passiveTCP(const char *service, int qlen);

#define  LINELEN    128

// Función para evitar zombies
void sigchld_handler(int sig) {
    // Recoge todos los procesos hijos terminados sin bloquear
    while (waitpid(-1, NULL, WNOHANG) > 0);
}


/* Envia cmds FTP al servidor, recibe respuestas y las despliega */
void sendCmd(int s, char *cmd, char *res) {
  int n;
  int len = strlen(cmd);

  cmd[len] = '\r';                /* formatear cmd FTP: \r\n al final */
  cmd[len+1] = '\n';
  n = write(s, cmd, len+2);       /* envia cmd por canal de control */
  if (n < 0) {
      errexit("Error al escribir comando: %s", strerror(errno));
  }

  n = read (s, res, LINELEN - 1);  /* lee respuesta del svr */
  if (n < 0) {
      errexit("Error al leer respuesta: %s", strerror(errno));
  }

  res[n] = '\0';                  /* asegura terminador de string */
  printf ("%s", res); // Muestra la respuesta
}

/* envia cmd PASV; recibe IP,pto del SVR; se conecta al SVR y retorna sock conectado */
int pasivo (int s){
  int sdata;                      /* socket para conexion de datos */
  int nport;                      /* puerto (en numeros) en SVR */
  char cmd[128], res[128], *p;    /* comando y respuesta FTP */
  char host[64], port[8];         /* host y port del SVR (como strings) */
  int h1,h2,h3,h4,p1,p2;          /* octetos de IP y puerto del SVR */

  sprintf (cmd, "PASV");
  sendCmd(s, cmd, res);

  if (res[0] != '2') {
      printf("Error en PASV: %s\n", res);
      return -1;
  }
  p = strchr(res, '(');
  if (p == NULL) {
      printf("Respuesta PASV inválida: %s\n", res);
      return -1;
  }

  sscanf(p+1, "%d,%d,%d,%d,%d,%d", &h1,&h2,&h3,&h4,&p1,&p2);

  snprintf(host, 64, "%d.%d.%d.%d", h1,h2,h3,h4);
  nport = p1*256 + p2;
  snprintf(port, 8, "%d", nport);

  sdata = connectTCP(host, port);

  return sdata;
}

void ayuda () {
  printf ("\nCliente FTP Concurrente. Comandos disponibles:\n \
    --- Comandos basicos  ---\n \
    help              - despliega este texto\n \
    dir               - lista el directorio actual del servidor (LIST, PASV)\n \
    get <archivo>     - copia el archivo desde el servidor al cliente (RETR, PASV) - CONCURRENTE\n \
    put <file>        - copia el archivo desde el cliente al servidor (STOR, PASV) - CONCURRENTE\n \
    pput <file>       - copia el archivo desde el cliente al servidor (STOR, PORT) - CONCURRENTE\n \
    cd <dir>          - cambia al directorio dir en el servidor (CWD)\n \
    quit              - finaliza la sesion FTP (QUIT)\n\n \
    --- Comandos Extra (Gestión) ---\n \
    pwd               - muestra el directorio de trabajo actual (PWD)\n \
    sys               - muestra el sistema operativo del servidor (SYST)\n \
    noop              - comando vacío para mantener la conexión activa (NOOP)\n \
    mkdir <dir>       - crea un nuevo directorio en el servidor (MKD)\n \
    rmdir <dir>       - elimina un directorio vacío en el servidor (RMD)\n \
    delete <file>     - elimina un archivo en el servidor (DELE)\n \
    rename <orig> <new> - renombra un archivo o directorio (RNFR/RNTO)\n \
    rest <offset>     - prepara el servidor para reanudar una transferencia desde <offset> (REST)\n\n");
}

void salir (char *msg) {
  printf ("%s\n", msg);
  exit (1);
}

int main(int argc, char *argv[]) {
  char  *host = "localhost";
  char  *service = "ftp";
  char  cmd[LINELEN], res[LINELEN];
  char  data[LINELEN];
  char  user[32], *pass, prompt[LINELEN], *ucmd, *arg;
  int   s, s1 = 0, sdata, n;
  FILE  *fp;
  struct  sockaddr_in addrSvr;
  unsigned int alen;

  switch (argc) {
  case 1:
    host = "localhost";
    break;
  case 3:
    service = argv[2];
    /* FALL THROUGH */
  case 2:
    host = argv[1];
    break;
  default:
    fprintf(stderr, "Uso: TCPftp [host [port]]\n");
    exit(1);
  }

  s = connectTCP(host, service);

  // 1. Configurar manejador de SIGCHLD
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDWAIT;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
      perror("sigaction");
  }


  n = read (s, res, LINELEN-1);
  res[n] = '\0';
  printf ("%s", res);

  // Loop de autenticación
  while (1) {
    printf ("Please enter your username: ");
    if (scanf ("%s", user) != 1) exit(1);
    sprintf (cmd, "USER %s", user);
    sendCmd(s, cmd, res);

    pass = getpass("Enter your password: ");
    sprintf (cmd, "PASS %s", pass);
    sendCmd(s, cmd, res);

    if ((res[0]-'0')*100 + (res[1]-'0')*10 + (res[2]-'0') == 230) break;
  }

  ayuda();

  while (1) {
    printf ("ftp> ");
    if (fgets(prompt, sizeof(prompt), stdin) != NULL) {
      prompt[strcspn(prompt, "\n")] = 0;

      ucmd = strtok (prompt, " ");

      if (ucmd == NULL) continue;

      if (strcmp(ucmd, "dir") == 0) {
        sdata = pasivo(s);
        if (sdata < 0) continue;

        sprintf (cmd, "LIST");
        sendCmd(s, cmd, res);

        if (res[0] == '1') {
            while ((n = recv(sdata, data, LINELEN, 0)) > 0) {
                fwrite(data, 1, n, stdout);
            }
            close(sdata);

            n = read (s, res, LINELEN);
            res[n] = '\0';
            printf ("%s", res);
        } else {
            close(sdata);
        }

      // --- BLOQUE 'get' (RETR) CONCURRENTE (Modo Pasivo) ---
      } else if (strcmp(ucmd, "get") == 0) {
        arg = strtok (NULL, " ");
        if (arg == NULL) { printf("Uso: get <archivo>\n"); continue; }

        sdata = pasivo(s);
        if (sdata < 0) continue;

        sprintf (cmd, "RETR %s", arg);
        sendCmd(s, cmd, res);

        if (res[0] != '1') {
            close(sdata);
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            close(sdata);
            continue;
        } else if (pid == 0) {
            // PROCESO HIJO
            fp = fopen(arg, "wb");
            if (fp == NULL) {
                perror("fopen local file failed (child)");
                close(sdata);
                exit(1);
            }

            while ((n = recv(sdata, data, LINELEN, 0)) > 0) {
                if (fwrite(data, 1, n, fp) != n) {
                    perror("fwrite failed (child)");
                    break;
                }
            }
            printf("Transferencia de descarga de '%s' finalizada (Hijo).\n", arg);

            fclose(fp);
            close(sdata);
            exit(0);
        } else {
            // PROCESO PADRE
            close(sdata);
            printf("Transferencia de descarga de '%s' iniciada en segundo plano.\n", arg);

            n = read (s, res, LINELEN);
            res[n] = '\0';
            printf ("Respuesta del servidor de control: %s", res);
        }

      // --- BLOQUE 'put' (STOR) CONCURRENTE (Modo Pasivo) ---
      } else if (strcmp(ucmd, "put") == 0) {
        arg = strtok (NULL, " ");
        if (arg == NULL) { printf("Uso: put <archivo>\n"); continue; }

        fp = fopen(arg, "rb");
        if (fp == NULL) {perror ("Open local file"); continue;}

        sdata = pasivo(s);
        if (sdata < 0) { fclose(fp); continue; }

        sprintf (cmd, "STOR %s", arg);
        sendCmd(s, cmd, res);

        if (res[0] != '1') {
            close(sdata);
            fclose(fp);
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            close(sdata);
            fclose(fp);
            continue;
        } else if (pid == 0) {
            // PROCESO HIJO
            while ((n = fread (data, 1, LINELEN, fp)) > 0) {
                send (sdata, data, n, 0);
            }
            printf("Transferencia de subida de '%s' finalizada (Hijo).\n", arg);

            fclose(fp);
            close(sdata);
            exit(0);
        } else {
            // PROCESO PADRE
            fclose(fp);
            close(sdata);

            printf("Subida de '%s' iniciada en segundo plano.\n", arg);

            n = read (s, res, LINELEN);
            res[n] = '\0';
            printf ("Respuesta del servidor de control: %s", res);
        }

      // --- BLOQUE 'pput' (STOR) CONCURRENTE (Modo Activo/PORT) ---
      } else if (strcmp(ucmd, "pput") == 0) {
        arg = strtok (NULL, " ");
        if (arg == NULL) { printf("Uso: pput <archivo>\n"); continue; }

        fp = fopen(arg, "rb");
        if (fp == NULL) {perror ("Open local file"); continue;}

        if (s1==0) {
            s1 = passiveTCP ("0", 5);
            if (s1 < 0) { perror("Error en passiveTCP"); fclose(fp); continue; }
        }

        struct sockaddr_in local_addr;
        socklen_t addr_len = sizeof(local_addr);
        if (getsockname(s1, (struct sockaddr *)&local_addr, &addr_len) == -1) {
            perror("getsockname"); fclose(fp); continue;
        }

        char *ip_str = inet_ntoa(local_addr.sin_addr);
        int port_n = ntohs(local_addr.sin_port);

        char port_cmd_arg[64];
        char *temp_ip = strdup(ip_str);
        char *p_temp = temp_ip;
        while (*p_temp) {
            if (*p_temp == '.') *p_temp = ',';
            p_temp++;
        }

        int p1 = port_n / 256;
        int p2 = port_n % 256;

        sprintf (port_cmd_arg, "%s,%d,%d", temp_ip, p1, p2);
        free(temp_ip);

        sprintf (cmd, "PORT %s", port_cmd_arg);
        sendCmd(s, cmd, res);
        if (res[0] != '2') {
            fclose(fp);
            continue;
        }

        sprintf (cmd, "STOR %s", arg);
        sendCmd(s, cmd, res);
        if (res[0] != '1') {
            fclose(fp);
            continue;
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            fclose(fp);
            continue;
        } else if (pid == 0) {
            // PROCESO HIJO
            sdata = accept(s1, (struct sockaddr *)&addrSvr, &addr_len);
            if (sdata < 0) {
                perror("accept failed (child)");
                fclose(fp);
                exit(1);
            }

            while ((n = fread (data, 1, LINELEN, fp)) > 0) {
                send (sdata, data, n, 0);
            }
            printf("Transferencia de subida de '%s' (PORT) finalizada (Hijo).\n", arg);

            fclose(fp);
            close(sdata);
            exit(0);
        } else {
            // PROCESO PADRE
            fclose(fp);

            printf("Subida de '%s' (PORT) iniciada en segundo plano.\n", arg);

            n = read (s, res, LINELEN);
            res[n] = '\0';
            printf ("Respuesta del servidor de control: %s", res);
        }

      // --- BLOQUE 'cd' (CWD) ---
      } else if (strcmp(ucmd, "cd") == 0) {
        arg = strtok (NULL, " ");
        if (arg == NULL) { printf("Uso: cd <directorio>\n"); continue; }
        sprintf (cmd, "CWD %s", arg);
        sendCmd(s, cmd, res);

      // ---------------------------------------------
      // --- INICIO: COMANDOS EXTRA Y GESTIÓN ---
      // ---------------------------------------------

      // PWD (Imprimir directorio de trabajo)
      } else if (strcmp(ucmd, "pwd") == 0) {
          sprintf (cmd, "PWD");
          sendCmd(s, cmd, res);

      // SYST (Sistema Operativo)
      } else if (strcmp(ucmd, "sys") == 0) {
          sprintf (cmd, "SYST");
          sendCmd(s, cmd, res);

      // NOOP (No Operación)
      } else if (strcmp(ucmd, "noop") == 0) {
          sprintf (cmd, "NOOP");
          sendCmd(s, cmd, res);

      // MKD (Crear directorio)
      } else if (strcmp(ucmd, "mkdir") == 0) {
          arg = strtok (NULL, " ");
          if (arg == NULL) { printf("Uso: mkdir <directorio>\n"); continue; }
          sprintf (cmd, "MKD %s", arg);
          sendCmd(s, cmd, res);

      // RMD (Eliminar Directorio)
      } else if (strcmp(ucmd, "rmdir") == 0) {
          arg = strtok (NULL, " ");
          if (arg == NULL) { printf("Uso: rmdir <directorio>\n"); continue; }
          sprintf (cmd, "RMD %s", arg);
          sendCmd(s, cmd, res);

      // DELE (Eliminar archivo)
      } else if (strcmp(ucmd, "delete") == 0) {
          arg = strtok (NULL, " ");
          if (arg == NULL) { printf("Uso: delete <archivo>\n"); continue; }
          sprintf (cmd, "DELE %s", arg);
          sendCmd(s, cmd, res);

      // RNFR/RNTO (Renombrar)
      } else if (strcmp(ucmd, "rename") == 0) {
          char *oldname = strtok (NULL, " ");
          char *newname = strtok (NULL, " ");

          if (oldname == NULL || newname == NULL) {
              printf("Uso: rename <nombre_original> <nombre_nuevo>\n");
              continue;
          }

          // Paso 1: RNFR (Rename From)
          sprintf (cmd, "RNFR %s", oldname);
          sendCmd(s, cmd, res);

          // Verificar código de "Ready for RNTO" (generalmente 350)
          if ((res[0]-'0')*100 + (res[1]-'0')*10 + (res[2]-'0') == 350) {
              // Paso 2: RNTO (Rename To)
              sprintf (cmd, "RNTO %s", newname);
              sendCmd(s, cmd, res);
          } else {
              printf("No se pudo iniciar la operación de renombrado.\n");
          }

      // REST (Reiniciar/Reanudar transferencia)
      } else if (strcmp(ucmd, "rest") == 0) {
          arg = strtok (NULL, " ");
          if (arg == NULL) { printf("Uso: rest <offset_en_bytes>\n"); continue; }

          if (strspn(arg, "0123456789") != strlen(arg)) {
              printf("El offset debe ser un número entero.\n");
              continue;
          }

          sprintf (cmd, "REST %s", arg);
          sendCmd(s, cmd, res);

      // ---------------------------------------------
      // --- FIN: COMANDOS DE EXTRA Y GESTIÓN ---
      // ---------------------------------------------

      } else if (strcmp(ucmd, "quit") == 0) {
        sprintf (cmd, "QUIT");
        sendCmd(s, cmd, res);
        exit (0);

      } else if (strcmp(ucmd, "help") == 0) {
        ayuda();

      } else {
        printf("%s: comando no implementado.\n", ucmd);
      }
    }
  }
}
