/**
 * @file main.c
 * @brief GATT server main function.
 *
 * This application demonstrates the usage of the GATT server library by
 * providing a DBus connection to the profile using BlueZ DBus API.
 */

#include"hrp.h"
static GMainLoop *main_loop;                                                    
static DBusConnection *connection; 

/**                                                                             
 * @brief This function allocates a block of memory of the spesified size.      
 *                                                                              
 * @param size for number of bytes to allocate.                                 
 */                                                                             
void *util_malloc(size_t size)                                                  
{                                                                               
    if (__builtin_expect(!!size, 1)) {                                      
        void *ptr;                                                      
                                                                               
        ptr = malloc(size);                                             
        if (ptr)                                                        
            return ptr;                                             
                                                                                
        fprintf(stderr, "failed to allocate %zu bytes\n", size);        
        abort();                                                        
     }                                                                       
     return NULL;                                                            
}

/**                                                                             
 * @brief This function will creates a copy of memory block.                    
 *                                                                              
 * @param src   A pointer to source memory block.                               
 * @param size  The size of the memory block.                                   
 */                                                                             
void *util_memdup(const void *src, size_t size)                                 
{                                                                               
    void *cpy;                                                              
    if (!src || !size)                                                      
        return NULL;                                                    
    cpy = util_malloc(size);                                                
    if (!cpy)                                                               
        return NULL;                                                    
    memcpy(cpy, src, size);                                                 
    return cpy;                                                             
}                         

/**
 * @brief This function is used as a callback for proxy-added signal when a new
 * GDBusProxy object is added.
 *
 * @param proxy     A pointer to GSBusProxy object.
 * @param user_data A pointer to the user defined data.
 */
static void proxy_added_cb(GDBusProxy *proxy, void *user_data)
{
	const char *iface;

	iface = g_dbus_proxy_get_interface(proxy);

	if (g_strcmp0(iface, GATT_MGR_IFACE))
		return;

	register_app(proxy);
}

/**
 * @brief This function is the callback function. It is called when a signal is
 * received.
 *
 * @param channel       A pointer to GIOChannel.
 * @param cond          The condition that triggered the signal handler.
 * @param user_data     A pointer to user defined data.
 *
 * @return TRUE indicate the signal handling should continue.
 */
static gboolean signal_handler(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	static bool __terminated = false;
	struct signalfd_siginfo si;
	ssize_t result;
	int fd;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP))
		return FALSE;

	fd = g_io_channel_unix_get_fd(channel);

	result = read(fd, &si, sizeof(si));
	if (result != sizeof(si))
		return FALSE;

	switch (si.ssi_signo) {
	case SIGINT:
	case SIGTERM:
		if (!__terminated) {
			printf("Terminating\n");
			g_main_loop_quit(main_loop);
		}

		__terminated = true;
		break;
	}

	return TRUE;
}

/**
 * @brief This function set up a signal handler usinh signalfd.
 */
static guint setup_signalfd(void)
{
	GIOChannel *channel;
	guint source;
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
		perror("Failed to set signal mask");
		return 0;
	}

	fd = signalfd(-1, &mask, 0);
	if (fd < 0) {
		perror("Failed to create signal descriptor");
		return 0;
	}

	channel = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(channel, TRUE);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	source = g_io_add_watch(channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				signal_handler, NULL);

	g_io_channel_unref(channel);

	return source;
}

/**                                                                             
 * @brief callback function for handling data.                                   
 *                                                                              
 * This function prints the data and the size of the data which is write to the 
 * characteristic and the descriptor.                                            
 *                                                                              
 * @param data a pointer to const char representing the data to be processed    
 * @param size size of the data.                                                 
 */        
void callback(const char *data, int size)                                                                                                  
{                                                                               
    printf("%s\n", data);                                                       
    printf("SIZE: %d\n", size);                                                 
}                             

/**
 * @brief main function initializes and runs the main loop of the program, which
 * handles D-Bus events and signals.
 *
 * @return 0 indicates successful program execution.
 */
int main(int argc, char *argv[])
{
	GDBusClient *client;
	guint signal;

	signal = setup_signalfd();
	if (signal == 0)
		return -errno;

	connection = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, NULL);

	main_loop = g_main_loop_new(NULL, FALSE);

	g_dbus_attach_object_manager(connection);

	printf("gatt-service unique name: %s\n",
				dbus_bus_get_unique_name(connection));

	create_services_one(connection);

	client = g_dbus_client_new(connection, "org.bluez", "/");

	g_dbus_client_set_proxy_handlers(client, proxy_added_cb, NULL, NULL,
									NULL);

	g_main_loop_run(main_loop);

	g_dbus_client_unref(client);

	g_source_remove(signal);

	g_slist_free_full(services, g_free);
	dbus_connection_unref(connection);

	return 0;
}


