# **Informe del Trabajo Práctico 5: Diseño y Construcción de un Character Device Driver**

**Autores:**  
* Viale, Sofia
* Daniel, Tomas

**Materia:** Sistemas de Computacion  
**Grupo:** *RAMbos*

---

## 1. Resumen del Proyecto

El presente trabajo práctico tuvo como objetivo principal el diseño, la construcción y la prueba de un sistema completo que demuestra la interacción entre el espacio del kernel y el espacio de usuario en un sistema operativo Linux. Se implementó un **Driver de Dispositivo de Caracteres (Character Device Driver - CDD)** en lenguaje C, capaz de generar dos tipos de señales periódicas. Complementariamente, se desarrolló una **aplicación de usuario** en Python con una interfaz gráfica (GUI) para visualizar en tiempo real una de las señales generadas, permitiendo al usuario seleccionar cuál de ellas monitorear.

## 2. Objetivos

Los objetivos específicos a cumplir, según la consigna del trabajo práctico, fueron los siguientes:

*   **Diseñar y construir un CDD** que actúe como un sensor virtual.
*   El CDD debe ser capaz de generar (sensar) **dos señales externas** distintas.
*   El período de sensado debe ser de **un (1) segundo**.
*   Desarrollar una **aplicación a nivel de usuario** que pueda leer los datos del CDD.
*   La aplicación debe permitir al usuario **indicarle al CDD cuál de las dos señales leer**.
*   La aplicación debe **graficar la señal seleccionada** en función del tiempo.
*   El gráfico debe incluir etiquetas claras: tipo de señal, unidades en el eje de ordenadas y tiempo en el eje de abscisas.
*   Al cambiar de señal, el gráfico debe **resetearse** y adaptarse a la nueva medición.

## 3. Metodología y Arquitectura del Sistema

Para cumplir con los objetivos, se diseñó una arquitectura cliente-servidor, donde el driver en el kernel actúa como servidor de datos y la aplicación de usuario como cliente.

### 3.1. Componente 1: Driver de Dispositivo de Caracteres (Kernel Space)

Este componente fue desarrollado en **lenguaje C** y compilado como un módulo del kernel de Linux (`.ko`). Sus responsabilidades principales son:

*   **Creación del Dispositivo:** Al cargarse, el driver crea dinámicamente un archivo de dispositivo en el sistema de archivos virtual, `/dev/signal_generator`, que sirve como punto de comunicación con el espacio de usuario.
*   **Generación Periódica de Datos:** Se utiliza un **temporizador del kernel (`ktimer`)** configurado para dispararse cada segundo. La función de callback del temporizador se encarga de calcular el nuevo valor de la señal.
*   **Lógica de Señales:** La generación de las dos señales está implementada directamente en C:
    1.  **Señal Cuadrada:** Alterna entre los valores `1` y `-1` con un período de 4 segundos.
    2.  **Señal Triangular:** Genera una onda que asciende de `0` a `2` y desciende de vuelta a `0`, también con un período de 4 segundos.
*   **Interfaz de Archivo (`file_operations`):** Se implementaron las operaciones estándar para un dispositivo de caracteres:
    *   `open()`: Permite a la aplicación de usuario abrir el archivo del dispositivo.
    *   `read()`: Envía el último par de datos (tiempo, valor) al espacio de usuario cuando la aplicación lo solicita.
    *   `write()`: Permite a la aplicación de usuario enviar un comando al driver (un carácter '1' o '2') para seleccionar la señal a generar.
    *   `release()`: Se ejecuta cuando la aplicación cierra el archivo.
*   **Sincronización:** Se utiliza un `mutex` para proteger el acceso a las variables globales compartidas (como el valor actual y el tipo de señal), evitando condiciones de carrera entre la interrupción del temporizador y las llamadas desde el espacio de usuario.

### 3.2. Componente 2: Aplicación de Visualización (User Space)

La aplicación de usuario fue desarrollada en **Python 3**, utilizando las siguientes librerías:
*   **Tkinter:** Para la creación de la interfaz gráfica de usuario (ventana, botones y layout).
*   **Matplotlib:** Para la generación y actualización dinámica de los gráficos.
*   **OS:** Para las operaciones de bajo nivel con el sistema de archivos, como `open()`, `read()` y `write()` sobre el archivo de dispositivo `/dev/signal_generator`.

Sus responsabilidades son:
*   Proveer una **interfaz gráfica intuitiva** con botones para la selección de la señal.
*   **Abrir y gestionar la comunicación** con el driver del kernel.
*   **Leer datos periódicamente** del driver cada segundo.
*   **Actualizar el gráfico** en tiempo real con los nuevos datos recibidos.
*   **Reiniciar el gráfico** y la recolección de datos cuando el usuario cambia de señal.

## 4. Implementación y Pruebas

### 4.1. Compilación y Carga del Módulo

El módulo del kernel se compiló utilizando un `Makefile` que automatiza el proceso. Dado que el sistema de pruebas utiliza **Secure Boot**, fue necesario firmar el módulo `.ko` con una clave MOK previamente registrada en el sistema.

El `Makefile` fue configurado para realizar la firma automáticamente después de la compilación:
```makefile
# Makefile final, corregido y robusto para compilación y firma automática

# Nombre del objeto del módulo que queremos construir.
obj-m += signal_driver.o

# --- Configuración de la Firma Digital ---
MOK_KEY_PRIV := MOK_TP5.priv
MOK_KEY_PUB  := MOK_TP5.der
SIGN_FILE := $(shell find /lib/modules/$(shell uname -r)/build/scripts -name sign-file)

# --- Comandos Principales ---
all:
	@echo "==> [Paso 1/2] Compilando el módulo del kernel..."
	@make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	@echo "==> Compilación finalizada."
	@echo "==> [Paso 2/2] Buscando clave para firmar el módulo..."
	@if [ -f "$(MOK_KEY_PRIV)" ]; then \
		echo "    -> ¡Éxito! Clave privada '$(MOK_KEY_PRIV)' encontrada."; \
		echo "    -> Firmando '$(patsubst %.o,%.ko,$(obj-m))'..."; \
		sudo $(SIGN_FILE) sha256 $(MOK_KEY_PRIV) $(MOK_KEY_PUB) $(patsubst %.o,%.ko,$(obj-m)); \
		echo "==> ¡Módulo firmado exitosamente!"; \
	else \
		echo "    -> ¡ADVERTENCIA! No se encontró la clave privada '$(MOK_KEY_PRIV)'."; \
	fi

clean:
	@echo "==> Limpiando archivos de compilación..."
	@make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

La carga del módulo se realizó con los siguientes comandos, incluyendo el ajuste de permisos para el archivo de dispositivo:

```
# Cargar el módulo firmado en el kernel
sudo insmod signal_driver.ko

# Ajustar permisos para permitir el acceso desde la aplicación de usuario
sudo chmod 666 /dev/signal_generator
```

### 4.2. Resultados de la Ejecución

La aplicación de usuario se ejecutó exitosamente, logrando comunicarse con el driver y visualizar los datos.

#### 4.2.1. Visualización de la Señal Cuadrada

Al iniciar la aplicación, esta solicita por defecto la señal 1. El gráfico muestra correctamente la onda cuadrada alternando entre los valores 1 y -1, con un período de 4 segundos, tal como se programó en el driver.

![Senal Cuadrada](TP5-Python/img/SenalCuadrada.png)

    Figura 1: Visualización en tiempo real de la señal cuadrada generada por el CDD.

#### 4.2.2. Visualización de la Señal Triangular

Al presionar el botón "Señal 2 (Triangular)", la aplicación envía el comando correspondiente al driver. El gráfico se resetea y comienza a mostrar la nueva señal. Se puede observar la onda triangular que oscila entre 0 y 2, cumpliendo con la especificación.

![Senal Trangular](TP5-Python/img/SenalTraingular.png)

    Figura 2: Visualización de la señal triangular después de que el usuario solicitara el cambio.

---

## 5. Conclusiones

El trabajo práctico se completó de manera exitosa, cumpliendo con todos los requisitos de la consigna. Se logró implementar un sistema funcional que demuestra la arquitectura de comunicación entre el kernel y el espacio de usuario a través de un driver de dispositivo de caracteres.

El proyecto permitió afianzar conceptos clave de sistemas operativos, tales como:

    La programación de módulos del kernel en C.

    El manejo de temporizadores y la sincronización (`mutex`) en el kernel.

    La creación de interfaces de dispositivo a través de `file_operations`.

    La interacción de una aplicación de usuario con el kernel mediante llamadas al sistema (`open`, `read`, `write`).

    Los desafíos de seguridad en sistemas modernos, como la necesidad de firmar módulos para `Secure Boot`.

Este ejercicio práctico ha sido fundamental para comprender de manera aplicada la teoría de diseño de drivers y la estructura interna de un sistema operativo Linux.

---