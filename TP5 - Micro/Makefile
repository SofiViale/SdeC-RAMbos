# Makefile final, corregido y robusto para compilación y firma automática

# Nombre del objeto del módulo que queremos construir.
# make lo convertirá en signal_driver.ko
obj-m += signal_driver.o

# --- Configuración de la Firma Digital ---

# Nombres de tus archivos de clave.
# Asegúrate de que los archivos 'MOK_TP5.priv' y 'MOK_TP5.der'
# están en este mismo directorio.
MOK_KEY_PRIV := MOK_TP5.priv
MOK_KEY_PUB  := MOK_TP5.der

# Busca automáticamente la ruta completa del script de firma del kernel.
# Esto hace que el Makefile sea portable entre diferentes máquinas.
SIGN_FILE := $(shell find /lib/modules/$(shell uname -r)/build/scripts -name sign-file)


# --- Comandos Principales ---

# El comando 'all' es el que se ejecuta por defecto al escribir 'make'.
all:
	# 1. Llama al sistema de construcción del kernel para compilar nuestro módulo.
	@echo "==> [Paso 1/2] Compilando el módulo del kernel..."
	@make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	@echo "==> Compilación finalizada."
	
	# 2. Comprueba si la clave privada existe y, si es así, firma el módulo.
	@echo "==> [Paso 2/2] Buscando clave para firmar el módulo..."
	@if [ -f "$(MOK_KEY_PRIV)" ]; then \
		echo "    -> ¡Éxito! Clave privada '$(MOK_KEY_PRIV)' encontrada."; \
		echo "    -> Firmando '$(patsubst %.o,%.ko,$(obj-m))'..."; \
		sudo $(SIGN_FILE) sha256 $(MOK_KEY_PRIV) $(MOK_KEY_PUB) $(patsubst %.o,%.ko,$(obj-m)); \
		echo "==> ¡Módulo firmado exitosamente!"; \
	else \
		echo "    -> ¡ADVERTENCIA! No se encontró la clave privada '$(MOK_KEY_PRIV)'."; \
		echo "==> El módulo NO ha sido firmado. No se podrá cargar si Secure Boot está activo."; \
	fi

# El comando 'clean' se usa para borrar todos los archivos generados por la compilación.
clean:
	@echo "==> Limpiando archivos de compilación..."
	@make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	@echo "==> Limpieza finalizada."