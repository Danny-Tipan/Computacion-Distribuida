# Cliente FTP Concurrente en C 

Este proyecto implementa un cliente del Protocolo de Transferencia de Archivos (FTP) que soporta los modos de transferencia **Pasivo** y **Activo (PORT)**, con la capacidad de manejar transferencias de archivos de forma **concurrente** utilizando procesos (`fork()`).

## Características Principales

* **Concurrencia:** Permite que las transferencias de archivos (`get`/`put`/`pput`) se ejecuten en segundo plano (proceso hijo) mientras la conexión de control permanece activa (proceso padre).
* **Modos de Transferencia:** Soporte para **Modo Pasivo (PASV)** y **Modo Activo (PORT)**.
* **Comandos FTP Estándar:** Implementa `USER`, `PASS`, `RETR`, `STOR`, `LIST`, `QUIT`, y otros comandos de gestión.
* **Gestión de Zombis:** Implementación de un manejador `SIGCHLD` con `waitpid()` para evitar procesos zombis tras las transferencias concurrentes.

## Requisitos

* Compilador **GCC** (GNU Compiler Collection).
* Librerías estándar de red de C.
* Un servidor FTP activo (ej: **vsftpd**) para conectarse.

## Compilación del Proyecto

El proyecto se compila fácilmente usando el `Makefile` proporcionado:

1. Asegúrate de tener todos los archivos `.c` (incluyendo `TipanD-clienteFTP.c`, `connectTCP.c`, `errexit.c`, etc.) en el mismo directorio.
2. Ejecuta el comando `make` en la terminal:

    make

Esto generará el ejecutable llamado **`TipanD-clienteFTP`**.

## Uso del Cliente

### 1. Sintaxis

El programa acepta argumentos para especificar el host y el puerto.

    ./TipanD-clienteFTP [host [port]]

Tambien puede dejarlo sin argumentos y usara los definidos por defecto.

### 2. Comandos Internos

Una vez autenticado, puedes usar los siguientes comandos:

| Comando | Descripción | Concurrencia | Modo |
| :--- | :--- | :--- | :--- |
| **`dir`** | Lista el directorio (LIST). | No | Pasivo |
| **`get <archivo>`** | Descarga un archivo (RETR). | **Sí** | Pasivo |
| **`put <archivo>`** | Sube un archivo (STOR). | **Sí** | Pasivo |
| **`pput <archivo>`** | Sube un archivo (STOR). | **Sí** | Activo (PORT) |
| **`cd <dir>`** | Cambia de directorio (CWD). | No | - |
| **`quit`** | Cierra la sesión (QUIT). | No | - |

**Comandos de Gestión Adicionales:** `pwd`, `sys`, `noop`, `mkdir`, `rmdir`, `delete`, `rename`, `rest`.

## Consideraciones de WSL y Firewall

Si estás ejecutando el cliente y el servidor FTP (como vsftpd) bajo **WSL (Windows Subsystem for Linux)**, se debe tener en cuenta lo siguiente:

### 1. Conexión de Control (Puerto 21)

* Si **vsftpd** corre dentro de WSL, la conexión por defecto a **`localhost`** (`./TipanD-clienteFTP`) funcionará sin problemas.

### 2. Transferencias de Datos y Firewall

* El **Modo Pasivo** (`get`, `put`) requiere que el servidor (vsftpd) abra un rango de puertos altos (puertos de datos).
* El **Firewall de Windows** puede bloquear la conexión de datos, incluso si la conexión de control (puerto 21) es exitosa.
* **Solución:** Si las transferencias fallan, verifica que el **rango de puertos pasivos** configurado en `vsftpd.conf` (ej: 30000-30100) esté **abierto** en el Firewall de Windows para tráfico de entrada y salida.

## Limpieza

Para eliminar todos los archivos objeto (`.o`) y el ejecutable `TipanD-clienteFTP`:

    make clean
