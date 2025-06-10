#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "signal_generator"
#define CLASS_NAME  "signal_class"

// --- Variables Globales del Driver ---
static dev_t dev_num;
static struct class *signal_class = NULL;
static struct cdev signal_cdev;
static struct timer_list signal_timer;

// Variables para la generación de la señal
static int major_number;
static int selected_signal = 1; // 1 para cuadrada, 2 para triangular
static long g_current_time = 0;
static int current_value = 0;

// Mutex para proteger el acceso concurrente a las variables compartidas
static DEFINE_MUTEX(signal_mutex);

// --- Funciones del Driver ---

// Función que se ejecuta cada vez que el temporizador se dispara
void timer_callback(struct timer_list *t) {
    mutex_lock(&signal_mutex);

    // Generar el valor de la señal seleccionada
    if (selected_signal == 1) {
        // Señal cuadrada: 1 para t en [0,2), -1 para t en [2,4)
        current_value = ((g_current_time % 4) < 2) ? 1 : -1;
    } else {
        // Señal triangular: va de 0 a 2 y de vuelta a 0
        long temp_time = g_current_time % 4;
        current_value = 2 - abs(temp_time - 2);
    }
    
    g_current_time++;

    mutex_unlock(&signal_mutex);

    // Reactivar el temporizador para que se ejecute de nuevo en 1 segundo
    mod_timer(&signal_timer, jiffies + HZ);
}

// Llamada cuando la aplicación abre /dev/signal_generator
static int dev_open(struct inode *inodep, struct file *filep) {
    pr_info("SignalGenerator: Dispositivo abierto\n");
    return 0;
}

// Llamada cuando la aplicación lee desde /dev/signal_generator
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    char message[32];
    int message_len;

    mutex_lock(&signal_mutex);
    // Convertimos el valor y el tiempo a una cadena "tiempo,valor"
    message_len = snprintf(message, sizeof(message), "%ld,%d\n", g_current_time -1, current_value);
    mutex_unlock(&signal_mutex);
    
    // Copiamos los datos desde el kernel al espacio de usuario
    error_count = copy_to_user(buffer, message, message_len);

    if (error_count == 0) {
        pr_info("SignalGenerator: Enviado '%s' al usuario\n", message);
        return message_len;
    } else {
        pr_info("SignalGenerator: Error al enviar datos al usuario\n");
        return -EFAULT;
    }
}

// Llamada cuando la aplicación escribe en /dev/signal_generator
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char user_char;

    if (len > 0) {
        // Obtenemos el primer carácter del buffer de usuario
        if (copy_from_user(&user_char, buffer, 1)) {
            return -EFAULT;
        }

        mutex_lock(&signal_mutex);
        if (user_char == '1') {
            selected_signal = 1;
            pr_info("SignalGenerator: Señal cambiada a CUADRADA\n");
        } else if (user_char == '2') {
            selected_signal = 2;
            pr_info("SignalGenerator: Señal cambiada a TRIANGULAR\n");
        } else {
            pr_warn("SignalGenerator: Comando no válido '%c'\n", user_char);
        }
        mutex_unlock(&signal_mutex);
    }

    return len;
}

// Llamada cuando la aplicación cierra /dev/signal_generator
static int dev_release(struct inode *inodep, struct file *filep) {
    pr_info("SignalGenerator: Dispositivo cerrado\n");
    return 0;
}

// Estructura que mapea las operaciones de archivo a nuestras funciones
static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// --- Función de Inicialización del Módulo ---
static int __init signal_driver_init(void) {
    pr_info("SignalGenerator: Inicializando el módulo...\n");

    // 1. Asignar dinámicamente un major number
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        pr_err("SignalGenerator: No se pudo asignar major number\n");
        return -1;
    }
    major_number = MAJOR(dev_num);
    pr_info("SignalGenerator: Major number asignado: %d\n", major_number);

    // 2. Crear la clase del dispositivo
    signal_class = class_create(CLASS_NAME);
    if (IS_ERR(signal_class)) {
        unregister_chrdev_region(dev_num, 1);
        pr_err("SignalGenerator: No se pudo crear la clase del dispositivo\n");
        return PTR_ERR(signal_class);
    }
    pr_info("SignalGenerator: Clase de dispositivo creada correctamente\n");

    // 3. Crear el dispositivo en /dev/
    if (device_create(signal_class, NULL, dev_num, NULL, DEVICE_NAME) == NULL) {
        class_destroy(signal_class);
        unregister_chrdev_region(dev_num, 1);
        pr_err("SignalGenerator: No se pudo crear el dispositivo\n");
        return -1;
    }
    pr_info("SignalGenerator: Dispositivo creado en /dev/%s\n", DEVICE_NAME);

    // 4. Inicializar el character device y asociarlo con fops
    cdev_init(&signal_cdev, &fops);
    if (cdev_add(&signal_cdev, dev_num, 1) < 0) {
        device_destroy(signal_class, dev_num);
        class_destroy(signal_class);
        unregister_chrdev_region(dev_num, 1);
        pr_err("SignalGenerator: No se pudo añadir el cdev\n");
        return -1;
    }

    // 5. Inicializar y arrancar el temporizador del kernel
    timer_setup(&signal_timer, timer_callback, 0);
    mod_timer(&signal_timer, jiffies + HZ); // HZ es 1 segundo
    
    pr_info("SignalGenerator: Módulo cargado y temporizador iniciado.\n");
    return 0;
}

// --- Función de Salida del Módulo ---
static void __exit signal_driver_exit(void) {
    pr_info("SignalGenerator: Descargando el módulo...\n");
    
    del_timer_sync(&signal_timer);   // Detener el temporizador
    cdev_del(&signal_cdev);          // Eliminar el cdev
    device_destroy(signal_class, dev_num); // Eliminar el dispositivo
    class_destroy(signal_class);     // Eliminar la clase
    unregister_chrdev_region(dev_num, 1); // Liberar el major number
    
    pr_info("SignalGenerator: Módulo descargado.\n");
}

module_init(signal_driver_init);
module_exit(signal_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("RAMbos");
MODULE_DESCRIPTION("Un driver de caracteres que genera señales periódicas.");