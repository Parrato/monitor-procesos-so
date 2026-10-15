# Monitor de Procesos con Control

Proyecto de Sistemas Operativos desarrollado en C++ para Linux.

El programa implementa un monitor de procesos interactivo en terminal utilizando
directamente la información expuesta por Linux mediante `/proc`.

No utiliza `ps`, `top`, `htop`, `tasklist`, PowerShell ni librerías que entreguen
la lista de procesos o el porcentaje de CPU ya calculado.

## Requisitos

- Linux
- C++17
- GCC / G++
- Make
- ncurses

En Ubuntu:

```bash
sudo apt update
sudo apt install build-essential libncurses-dev