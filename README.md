# Cliente FTP Concurrente en C

Este proyecto implementa un cliente del Protocolo de Transferencia de Archivos (FTP) que soporta los modos de transferencia **Pasivo** y **Activo (PORT)**, con la capacidad de manejar transferencias de archivos de forma **concurrente** utilizando procesos (`fork()`).

## Características Principales

* **Concurrencia:** Permite que las transferencias de archivos (`get`/`put`/`pput`) se ejecuten en segundo plano (proceso hijo) mientras la conexión de control permanece activa (proceso padre).
* **Modos de Transferencia:** Soporte para **Modo Pasivo (PASV)** y **Modo Activo (PORT)**.
* **Gestión de Zombis:** Implementación de un manejador `SIGCHLD` con `waitpid()` para evitar procesos zombis tras las transferencias concurrentes.

---

## Compilación del Proyecto

El proyecto se compila fácilmente usando el `Makefile` proporcionado:

1.  Asegúrate de tener todos los archivos `.c` en el mismo directorio.
2.  Ejecuta el comando `make` en la terminal:

```bash
make
