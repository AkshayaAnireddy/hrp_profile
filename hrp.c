/**
 * @file hrp.c
 * @brief Implementation of a GATT server functionality of HRP(Heart Rate Profile).
 * 
 * This file contains the implementation of a GATT(Generic Attribute Profile) server
 * using the BlueZ DBus. This program defines several UUID's, Interfaces and properties
 * for GATT service, characteristic and descriptor. It also includes functions like for
 * handling property getter, setter, read, write, notify, register and creating service.
 */

#include "hrp.h"

/**
 * @brief This function is used asa callback to retrieve the UUID property value
 * of the descriptor.
 *
 * @param property  A pointer to the GDBusPropertyTable structure.
 * @param iter      A pointer to DBusMessageIter structure.
 * @param user_data A pointer to user defined data.
 *
 * @return TRUE indicates the property value retrieval was successful.
 */
static gboolean desc_get_uuid(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct descriptor *desc = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &desc->uuid);

	return TRUE;
}

/**
 * @brief This function is used as a callback to retrieve the characteristic
 * property value of a descriptor.
 *
 * @param property  A pointer to the GDBusPropertyTable structure.
 * @param iter      A pointer to DBusMessageIter structure.
 * @param user_data A pointer to user defined data.
 * 
 * @return TRUE indicates the property value retrieval was successful.
 */
static gboolean desc_get_characteristic(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct descriptor *desc = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
						&desc->chr->path);

	return TRUE;
}

/**
 * @brief This function is responsible for reading the value of a descriptor and
 * appending it to a D-Bus message iterator.
 *
 * @param desc  A pointer to the descriptor structure.
 * @param iter  A pointer to DBusMessageIter structure.
 *
 * @return true indicates the read operation was successful.
 */
static bool desc_read(struct descriptor *desc, DBusMessageIter *iter)
{
	DBusMessageIter array;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);

	if (desc->vlen && desc->value)
		dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
						&desc->value, desc->vlen);

	dbus_message_iter_close_container(iter, &array);

	return true;
}

/**
 * @brief This function is used as a callback to handle property access request
 * for the Value property of a descriptor.
 *
 * @param property  A pointer to GDBusPropertyTable structure.
 * @param iter      A pointer to DBusMessageIter structure.
 * @param user_data A pointer to the user defined data.
 *
 * @return This function returns the result of desc_read function which is a
 * boolean value indicating the success of the read operation.
 */
static gboolean desc_get_value(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct descriptor *desc = user_data;

	printf("Descriptor(%s): Get(\"Value\")\n", desc->uuid);

	return desc_read(desc, iter);
}

/**
 * @brief This function is used to handle the writing of a value to a descriptor.
 *
 * @param connection    A pointer to DBusConnection.
 * @param desc          A pointer to the descriptor structure.
 * @param value         A pointer to the buffer containing the value to be written.
 * @param len           Length of the value buffer.
 *
 */
static void desc_write(DBusConnection *connection, struct descriptor *desc, const uint8_t *value, int len)
{
	free(desc->value);
	desc->value = util_memdup(value, len);
	desc->vlen = len;

	callback(desc->value, desc->vlen);

	g_dbus_emit_property_changed(connection, desc->path,
					GATT_DESCRIPTOR_IFACE, "Value");
}

/**
 * @brief This function is used to parse a value from D-Bus message iterator and
 * retrieve the value and length of an array.
 *
 * @param iter  A pointer to DBusMessageIter structure.
 * @param value A pointer to a pointer that will upated with the address of value array.
 * @param len   A pointer to an integer that will update with the length of value array.
 *
 * @return 0 indicates the successful parsing the value.
 */
static int parse_value(DBusMessageIter *iter, const uint8_t **value, int *len)
{
	DBusMessageIter array;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(iter, &array);
	dbus_message_iter_get_fixed_array(&array, value, len);

	return 0;
}

/**
 * @brief This function is used to handle property setting request for Value
 * property of a descriptor.
 *
 * @param property  A pointer to GDBusPropertyTable structure.
 * @param iter      A pointer to DBusMessageIter structure.
 * @param id        The identifier of the pending property set operation.
 * @param user_data A pointer to the user defined data.
 */
static void desc_set_value(const GDBusPropertyTable *property,
				DBusMessageIter *iter,
				GDBusPendingPropertySet id, void *user_data)
{
	struct descriptor *desc = user_data;
	const uint8_t *value;
	int len;

	printf("Descriptor(%s): Set(\"Value\", ...)\n", desc->uuid);

	if (parse_value(iter, &value, &len)) {
		printf("Invalid value for Set('Value'...)\n");
		g_dbus_pending_property_error(id,
					"error" ".InvalidArguments",
					"Invalid arguments in method call");
		return;
	}

	desc_write(NULL, desc, value, len);

	g_dbus_pending_property_success(id);
}

/**
 * @brief This function is used to retrieve the properties of a descriptor.
 *
 * @param property  A pointer to the GDBusPropertyTable structure.
 * @param iter      A pointer to the DBusMessageIter structure.
 * @param data      A pointer to the user defined data.
 *
 * @return TRUE indicating the property retrieval was successful.
 */
static gboolean desc_get_props(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	struct descriptor *desc = data;
	DBusMessageIter array;
	int i;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_STRING_AS_STRING, &array);

	for (i = 0; desc->props[i]; i++)
		dbus_message_iter_append_basic(&array,
					DBUS_TYPE_STRING, &desc->props[i]);

	dbus_message_iter_close_container(iter, &array);

	return TRUE;
}

/**
 * @brief An array called desc_properties of type GDBusPropertyTable.
 */
static const GDBusPropertyTable desc_properties[] = {
	{ "UUID",		"s", desc_get_uuid },
	{ "Characteristic",	"o", desc_get_characteristic },
	{ "Value",		"ay", desc_get_value, desc_set_value, NULL },
	{ "Flags",		"as", desc_get_props, NULL, NULL },
	{ }
};

/**
 * @brief This function is a callback to retrieve the UUID property value of a 
 * characteristic.
 *
 * @param property  A pointer to GDBusPropertyTable structure.
 * @param iter      A pointer to DBusMessageIter structure.
 * @param user_data A pointer to user defined data.
 *
 * @return TRUE indicate the property value retrieval was successful.
 */
static gboolean chr_get_uuid(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct characteristic *chr = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &chr->uuid);

	return TRUE;
}

/**
 * @brief This function is used as a callback to retrieve the Service property
 * value of a characteristic.
 *
 * @param property  A pointer to GDBusPropertyTable structure.
 * @param iter      A pointer to DBusMessageIter structure.
 * @param user_data A pointer to user defined data. 
 *
 * @return TRUE indicate the property value retrieval was successful.
 */
static gboolean chr_get_service(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct characteristic *chr = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
							&chr->service);

	return TRUE;
}

/**
 * @brief This function is responsible for reading the value of a characteristic
 * and appending it to a D-Bus message iterator.
 *
 * @param chr   A pointer to characteristic structure.
 * @param iter  A pointer to DBusMessageIter structure.
 * @return true indicates the read operation was successful.
 */
static bool chr_read(struct characteristic *chr, DBusMessageIter *iter)
{
	DBusMessageIter array;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);

	dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
						&chr->value, chr->vlen);

	dbus_message_iter_close_container(iter, &array);

	return true;
}

/**
 * @brief This function is used as a callback to handle property access request
 * for the Value property of a characteristic.
 *
 * @param property  A pointer to GDBusPropertyTable structure.                  
 * @param iter      A pointer to DBusMessageIter structure.                 
 * @param user_data A pointer to user defined data.                         
 *                                                                              
 * @return This function will returns the result of the chr_read function.
 */
static gboolean chr_get_value(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct characteristic *chr = user_data;

	printf("Characteristic(%s): Get(\"Value\")\n", chr->uuid);

	return chr_read(chr, iter);
}

/**
 * @brief This function is used to retrieve the properties of a characteristic.
 *
 * @param property  A pointer to GDBusPropertyTable structure.                  
 * @param iter      A pointer to DBusMessageIter structure.                 
 * @param user_data A pointer to user defined data.                         
 *                                                                              
 * @return TRUE indicate the property value retrieval was successful.       
 */
static gboolean chr_get_props(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	struct characteristic *chr = data;
	DBusMessageIter array;
	int i;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_STRING_AS_STRING, &array);

	for (i = 0; chr->props[i]; i++)
		dbus_message_iter_append_basic(&array,
					DBUS_TYPE_STRING, &chr->props[i]);

	dbus_message_iter_close_container(iter, &array);

	return TRUE;
}

/**                                                                             
 * @brief This function is used to handle the writing of a value to a characteristic.
 *                                                                              
 * @param connection    A pointer to DBusConnection.                            
 * @param chr           A pointer to the characteristic structure.                  
 * @param value         A pointer to the buffer containing the value to be written.
 * @param len           Length of the value buffer.                             
 *                                                                              
 */
static void chr_write(DBusConnection *connection, struct characteristic *chr, const uint8_t *value, int len)
{
	free(chr->value);
	chr->value = util_memdup(value, len);
	chr->vlen = len;

	callback(chr->value, chr->vlen);

	g_dbus_emit_property_changed(connection, chr->path, GATT_CHR_IFACE,
								"Value");
}

/**                                                                             
 * @brief This function is used to handle property setting request for Value    
 * property of a characteristic.                                                    
 *                                                                              
 * @param property  A pointer to GDBusPropertyTable structure.                  
 * @param iter      A pointer to DBusMessageIter structure.                     
 * @param id        The identifier of the pending property set operation.       
 * @param user_data A pointer to the user defined data.                         
 */                
static void chr_set_value(const GDBusPropertyTable *property,
				DBusMessageIter *iter,
				GDBusPendingPropertySet id, void *user_data)
{
	struct characteristic *chr = user_data;
	const uint8_t *value;
	int len;

	printf("Characteristic(%s): Set('Value', ...)\n", chr->uuid);

	if (!parse_value(iter, &value, &len)) {
		printf("Invalid value for Set('Value'...)\n");
		g_dbus_pending_property_error(id,
					"ERROR_INTERFACE" ".InvalidArguments",
					"Invalid arguments in method call");
		return;
	}

	chr_write(NULL, chr, value, len);

	g_dbus_pending_property_success(id);
}

/**
 * @brief An array called chr_properties of type GDBusPropertyTable.  
 */
static const GDBusPropertyTable chr_properties[] = {
	{ "UUID",	"s", chr_get_uuid },
	{ "Service",	"o", chr_get_service },
	{ "Value",	"ay", chr_get_value, chr_set_value, NULL },
	{ "Flags",	"as", chr_get_props, NULL, NULL },
	{ }
};

/**
 * @brief This function is used as a callback to handle property access request
 * for Primary property of a service.
 *
 * @param property  A pointer to GDBusPropertyTable structure.                  
 * @param iter      A pointer to DBusMessageIter structure.                 
 * @param user_data A pointer to user defined data.                         
 *                                                                              
 * @return TRUE indicate the property value retrieval was successful.                     
 */
static gboolean service_get_primary(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	dbus_bool_t primary = TRUE;

	printf("Get Primary: %s\n", primary ? "True" : "False");

	dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &primary);

	return TRUE;
}

/**
 * @brief This function is used as a callback to handle property access request 
 * for UUID property of a service. 
 *
 * @param property  A pointer to GDBusPropertyTable structure.                  
 * @param iter      A pointer to DBusMessageIter structure.                     
 * @param user_data A pointer to user defined data.                             
 *                                                                              
 * @return TRUE indicate the property value retrieval was successful.
 */
static gboolean service_get_uuid(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	const char *uuid = user_data;

	printf("Get UUID: %s\n", uuid);

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &uuid);

	return TRUE;
}

/**                                                                             
 * @brief An array called service_properties of type GDBusPropertyTable.            
 */
static const GDBusPropertyTable service_properties[] = {
	{ "Primary", "b", service_get_primary },
	{ "UUID", "s", service_get_uuid },
	{ "Includes", "ao",},
	{ }
};

/**
 * @brief This function is used as a callback to handle the distraction of a
 * GATT characteristic interface.
 *
 * @param user_data A pointer to user defined data associated with chr interface.
 *
 */
static void chr_iface_destroy(gpointer user_data)
{
	struct characteristic *chr = user_data;

	g_free(chr->uuid);
	g_free(chr->service);
	free(chr->value);
	g_free(chr->path);
	g_free(chr);
}

/**
 * @brief This function is used as a callback to handle the distraction of a 
 * descriptor interface
 *
 * @param user_data A pointer to user defined data associated with desc interface.
 */
static void desc_iface_destroy(gpointer user_data)
{
	struct descriptor *desc = user_data;

	g_free(desc->uuid);
	free(desc->value);
	g_free(desc->path);
	g_free(desc);
}

/**
 * @brief This function is used to parse options from D-Bus message iterator.
 *
 * @param iter      A pointer to DBusMessageIter structure.
 * @param device    A pointer to a pointer to a string representing the device option.
 *
 * @return 0 indicates the successful parsing.
 */
static int parse_options(DBusMessageIter *iter, const char **device)
{
	DBusMessageIter dict;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		const char *key;
		DBusMessageIter value, entry;
		int var;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		var = dbus_message_iter_get_arg_type(&value);
		if (strcasecmp(key, "device") == 0) {
			if (var != DBUS_TYPE_OBJECT_PATH)
				return -EINVAL;
			dbus_message_iter_get_basic(&value, device);
			printf("Device: %s\n", *device);
		}

		dbus_message_iter_next(&dict);
	}

	return 0;
}

/**
 * @brief This function is used to handle D-Bus method call for reading
 * the value of a characteristic.
 *
 * @param conn      A pointer to the DBusConnection.
 * @param msg       A pointer to the DBusMessage.
 * @param user_data A pointer to the user defined data.
 *
 * @return The function will return the reply message.
 */
static DBusMessage *chr_read_value(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	struct characteristic *chr = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;
	const char *device;

	if (!dbus_message_iter_init(msg, &iter))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return g_dbus_create_error(msg, DBUS_ERROR_NO_MEMORY,
							"No Memory");

	dbus_message_iter_init_append(reply, &iter);

	chr_read(chr, &iter);

	return reply;
}

/**
 * @brief This function is used to handle a D-Bus method call for writing 
 * the value of a characteristic.
 *
 * @param conn      A pointer to DBusConnection.
 * @param msg       A pointer to DBusMessage.
 * @param user_data A pointer to user defined data.
 *
 * @return The function creates a new method return message using 
 * dbus_message_new_method_return and return it.
 */
static DBusMessage *chr_write_value(DBusConnection *conn, DBusMessage *msg,

							void *user_data)
{
	struct characteristic *chr = user_data;
	DBusMessageIter iter;
	const uint8_t *value;
	int len;
	const char *device;

	dbus_message_iter_init(msg, &iter);

	if (parse_value(&iter, &value, &len))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	chr_write(conn, chr, value, len);

	return dbus_message_new_method_return(msg);
}

/**
 * @brief This function is used to send the notification.
 *
 * @param conn      A pointer to DBusConnection.
 * @param user_data A pointer to user defined data.
 *
 * @return true on successful sending the notification.
 */
static gboolean send_notification(DBusConnection *conn, void *user_data)

{
    struct characteristic *chr = user_data;

    uint8_t notification[] = {0x33, 0x34, 0x35};

    chr_write(conn, chr, notification, sizeof(notification));

    return true;
}

/**
 * @brief This function is used to start the notification.
 *
 * @param conn      A pointer to DBusConnection.
 * @param msg       A pointer to DBusMessage.
 * @param user_data A pointer to user defined data.
 *
 * @return The function creates a new method return message using               
 * dbus_message_new_method_return and return it.
 */
static DBusMessage *chr_start_notify(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
    DBusMessage *notify;
    const GDBusPropertyTable *property;
    DBusMessageIter *iter;
    GDBusPendingPropertySet id;
    struct characteristics *chr = user_data;

    notify = dbus_message_new_method_return(msg);
    if (!notify)
	    return g_dbus_create_error(msg, DBUS_ERROR_NOT_SUPPORTED,
		    					"Not Supported");
    send_notification(conn, chr);
    return notify;
}

/**
 * @brief This function is used to stop the notification.
 *
 * @param conn      A pointer to DBusConnection.
 * @param msg       A pointer to DBusMessage.
 * @param user_data A pointer to user defined data.
 *
 * @return  returns GDBus error.
 */
static DBusMessage *chr_stop_notify(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{

    printf("Notification stopped\n");
	return g_dbus_create_error(msg, DBUS_ERROR_NOT_SUPPORTED,
							"Not Supported");
}

/**
 * @brief represents the methods associated with the characteristic.
 */
static const GDBusMethodTable chr_methods[] = {
	{ GDBUS_ASYNC_METHOD("ReadValue", GDBUS_ARGS({ "options", "a{sv}" }),
					GDBUS_ARGS({ "value", "ay" }),
					chr_read_value) },
	{ GDBUS_ASYNC_METHOD("WriteValue", GDBUS_ARGS({ "value", "ay" },
						{ "options", "a{sv}" }),
					NULL, chr_write_value) },
	{ GDBUS_ASYNC_METHOD("StartNotify", NULL, NULL, chr_start_notify) },
	{ GDBUS_METHOD("StopNotify", NULL, NULL, chr_stop_notify) },
	{ }
};

/**                                                                             
 * @brief This function is used to handle D-Bus method call for reading         
 * the value of a descriptor                                  
 *                                                                              
 * @param conn      A pointer to the DBusConnection.                            
 * @param msg       A pointer to the DBusMessage.                               
 * @param user_data A pointer to the user defined data.                         
 *                                                                              
 * @return The function will return the reply message.                          
 */      
static DBusMessage *desc_read_value(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	struct descriptor *desc = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;
	const char *device;

	if (!dbus_message_iter_init(msg, &iter))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return g_dbus_create_error(msg, DBUS_ERROR_NO_MEMORY,
							"No Memory");

	dbus_message_iter_init_append(reply, &iter);

	desc_read(desc, &iter);

	return reply;
}

/**                                                                             
 * @brief This function is used to handle D-Bus method call for writing         
 * the value of a characteristic.                                               
 *                                                                              
 * @param conn      A pointer to the DBusConnection.                            
 * @param msg       A pointer to the DBusMessage.                               
 * @param user_data A pointer to the user defined data.                         
 *                                                                              
 * @return The function creates a new method return message using               
 * dbus_message_new_method_return and return it. 
 */      
static DBusMessage *desc_write_value(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	struct descriptor *desc = user_data;
	DBusMessageIter iter;
	const char *device;
	const uint8_t *value;
	int len;

	if (!dbus_message_iter_init(msg, &iter))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_value(&iter, &value, &len))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	desc_write(conn, desc, value, len);

	return dbus_message_new_method_return(msg);
}

/**                                                                             
 * @brief represents the methods associated with the characteristic.            
 */     
static const GDBusMethodTable desc_methods[] = {
	{ GDBUS_ASYNC_METHOD("ReadValue", GDBUS_ARGS({ "options", "a{sv}" }),
					GDBUS_ARGS({ "value", "ay" }),
					desc_read_value) },
	{ GDBUS_ASYNC_METHOD("WriteValue", GDBUS_ARGS({ "value", "ay" },
						{ "options", "a{sv}" }),
					NULL, desc_write_value) },
	{ }
};

/**
 * @brief This function is used to register a GATT characteristic and a optional
 * descriptors on a D-Bus connection
 *
 * @param connection        A pointer to DBusConnection.
 * @param chr_uuid          A srting representing the UUID of chr.
 * @param value             A pointer to byte array representing the value of chr.
 * @param vlen              An int representing the length of the byte array.
 * @param props             A pointer to array of strings.
 * @param desc_uuid         A string representing the UUID of desc.
 * @param desc_props        A pointer to array of strings.
 * @param service_path      A string representing the D-Bus object path.
 *
 * @return This function will return TRUE indicating the registration successful.
 */
static gboolean register_characteristic(DBusConnection *connection, 
                        const char *chr_uuid,
						const uint8_t *value, int vlen,
						const char **props,
						const char *desc_uuid,
						const char **desc_props,
						const char *service_path)
{
	struct characteristic *chr;
	struct descriptor *desc;
	static int id = 1;

	chr = g_new0(struct characteristic, 1);
	chr->uuid = g_strdup(chr_uuid);
	chr->value = util_memdup(value, vlen);
	chr->vlen = vlen;
	chr->props = props;
	chr->service = g_strdup(service_path);
	chr->path = g_strdup_printf("%s/characteristic%d", service_path, id++);

	if (!g_dbus_register_interface(connection, chr->path, GATT_CHR_IFACE,
					chr_methods, NULL, chr_properties,
					chr, chr_iface_destroy)) {
		printf("Couldn't register characteristic interface\n");
		chr_iface_destroy(chr);
		return FALSE;
	}

	if (!desc_uuid)
		return TRUE;

	desc = g_new0(struct descriptor, 1);
	desc->uuid = g_strdup(desc_uuid);
	desc->chr = chr;
	desc->props = desc_props;
	desc->path = g_strdup_printf("%s/descriptor%d", chr->path, id++);

	if (!g_dbus_register_interface(connection, desc->path,
					GATT_DESCRIPTOR_IFACE,
					desc_methods, NULL, desc_properties,
					desc, desc_iface_destroy)) {
		printf("Couldn't register descriptor interface\n");
		g_dbus_unregister_interface(connection, chr->path,
							GATT_CHR_IFACE);

		desc_iface_destroy(desc);
		return FALSE;
	}

	return TRUE;
}

/**
 * @brief This function is used to register a GATT service on a D-Bus connection.
 *
 * @param connection    A pointer to DBusConnection.
 * @param uuid          A string representing the service UUID
 *
 * @return  This function will returns the dynamically generated path for the service.
 */
static char *register_service(DBusConnection *connection, const char *uuid)
{
	static int id = 1;
	char *path;

	path = g_strdup_printf("/service%d", id++);
	if (!g_dbus_register_interface(connection, path, GATT_SERVICE_IFACE,
				NULL, NULL, service_properties,
				g_strdup(uuid), g_free)) {
		printf("Couldn't register service interface\n");
		g_free(path);
		return NULL;
	}

	return path;
}

/**
 * @brief This function is used to create andregister a set of GATT services,
 * along with their characteristics and descriptors on a D-Bus connection.
 *
 * @param connection    A pointer to DBusConnection.
 */
void create_services_one(DBusConnection *connection)
{
	char *service_path;
	uint8_t level = 0;

	service_path = register_service(connection, HRP_UUID);
	if (!service_path)
		return;

	/* Add Heart rate measurement characteristic to Heart rate service */
	if (!register_characteristic(connection, HR_MSRMT_CHR_UUID,
						&level, sizeof(level),
					    hrs_hr_msrmt_props,
						CLIENT_CHR_CONFIG_DESCRIPTOR_UUID,
						ccc_desc_props,
						service_path)) {
		printf("Couldn't Heart rate measurement characteristic (HRS)\n");
		g_dbus_unregister_interface(connection, service_path,
							GATT_SERVICE_IFACE);
		g_free(service_path);
		return;
	}

	if (!register_characteristic(connection, BODY_SENSOR_LOC_CHR_UUID,                             
	                    &level, sizeof(level),                                  
	                    hrs_body_sensor_loc_props,                                     
	                    NULL,                      
	                    NULL,                                         
	                    service_path)) {                                        
	     printf("Couldn't register body sensor location characteristic (HRS)\n");         
	     g_dbus_unregister_interface(connection, service_path,                   
	                         GATT_SERVICE_IFACE);                                
	     g_free(service_path);                                                   
	     return;                                                                 
	}                

    if (!register_characteristic(connection, HR_CTRL_PT_CHR_UUID,                             
                        &level, sizeof(level),                                  
                        hrs_hr_ctrl_pt_props,                                     
                        NULL,                      
                        NULL,                                         
                        service_path)) {                                        
       printf("Couldn't register Heart rate control point characteristic (HRS)\n");         
       g_dbus_unregister_interface(connection, service_path,                   
                             GATT_SERVICE_IFACE);                                
       g_free(service_path);                                                   
       return;                                                                 
    }                

	services = g_slist_prepend(services, service_path);
	printf("Registered service: %s\n", service_path);
}

/**
 * @brief This function is used as a callback for handling the replay of a
 * RegisterApplication D-Bus method call.
 *
 * @param reply     A pointer to DBusMessage.
 * @param user_data A pointer to user defined data.
 */
static void register_app_reply(DBusMessage *reply, void *user_data)
{
	DBusError derr;

	dbus_error_init(&derr);
	dbus_set_error_from_message(&derr, reply);

	if (dbus_error_is_set(&derr))
		printf("RegisterApplication: %s\n", derr.message);
	else
		printf("RegisterApplication: OK\n");

	dbus_error_free(&derr);
}

/**
 * @brief This function is used to setup the arguments for a RegisterApplication
 * on D-Bus connection
 *
 * @param iter      A pointer to DBusMessageIter.
 * @param user_data A pointer to user defined data.
 */
static void register_app_setup(DBusMessageIter *iter, void *user_data)
{
	const char *path = "/";
	DBusMessageIter dict;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}", &dict);

	/* TODO: Add options dictionary */

	dbus_message_iter_close_container(iter, &dict);
}

/**
 * @brief This function is used to make method call to the RegisterAplication
 * method on a GDBusProxy object.
 *
 * @param proxy     A pointer to GDBusProxy object representing the D-Bus proxy.
 */
void register_app(GDBusProxy *proxy)
{
	if (!g_dbus_proxy_method_call(proxy, "RegisterApplication",
					register_app_setup, register_app_reply,
					NULL, NULL)) {
		printf("Unable to call RegisterApplication\n");
		return;
	}
}



