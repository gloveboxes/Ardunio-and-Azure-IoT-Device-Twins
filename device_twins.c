#include "device_twins.h"

void SetDesiredState(JSON_Object *desiredProperties, DeviceTwinBinding *deviceTwinBinding, int correlationId);
void DeviceTwinsReportStatusCallback(int result, void *context);
bool TwinReportState(DeviceTwinBinding *deviceTwinBinding);

bool (*_updateReportedStateCallback)(DeviceTwinBinding *deviceTwinBinding, char *reportedState) = NULL;
DeviceTwinBinding **_deviceTwins = NULL;
size_t _deviceTwinCount = 0;

void OpenDeviceTwinSet(DeviceTwinBinding *deviceTwins[], size_t deviceTwinCount)
{
	_deviceTwins = deviceTwins;
	_deviceTwinCount = deviceTwinCount;

	for (int i = 0; i < _deviceTwinCount; i++)
	{
		OpenDeviceTwin(_deviceTwins[i]);
	}
}

void CloseDeviceTwinSet(void)
{
	for (int i = 0; i < _deviceTwinCount; i++)
	{
		CloseDeviceTwin(_deviceTwins[i]);
	}
}

bool OpenDeviceTwin(DeviceTwinBinding *deviceTwinBinding)
{
	if (deviceTwinBinding->twinType == TYPE_UNKNOWN)
	{
		// Device Twin missing type information.
		// Example .twinType=TYPE_BOOL
		// Valid types include TYPE_BOOL, TYPE_INT, TYPE_FLOAT, TYPE_STRING.
		return false;
	}

	// types JSON and String allocated dynamically when called in azure_iot.c
	switch (deviceTwinBinding->twinType)
	{
	case TYPE_INT:
		deviceTwinBinding->twinState = malloc(sizeof(int));
		break;
	case TYPE_FLOAT:
		deviceTwinBinding->twinState = malloc(sizeof(float));
		break;
	case TYPE_BOOL:
		deviceTwinBinding->twinState = malloc(sizeof(bool));
		break;
	case TYPE_STRING:
		deviceTwinBinding->twinState = NULL;
	default:
		break;
	}
	return true;
}

void CloseDeviceTwin(DeviceTwinBinding *deviceTwinBinding)
{
	if (deviceTwinBinding->twinState != NULL)
	{
		free(deviceTwinBinding->twinState);
		deviceTwinBinding->twinState = NULL;
	}
}

DeviceTwinBinding *GetDeviceTwinBindingByRequestId(int requestId)
{
	for (int i = 0; i < _deviceTwinCount; i++)
	{
		if (_deviceTwins[i]->requestId == requestId)
		{
			return _deviceTwins[i];
		}
	}
	return NULL;
}

/// <summary>
///     Callback invoked when a to process Device Twin
/// </summary>
/// <param name="payload">contains the Device Twin JSON document (desired and reported)</param>
/// <param name="payloadSize">size of the Device Twin JSON document</param>
void DeviceTwinHandler(const unsigned char *payload, size_t payloadSize, bool (*updateReportedStateCallback)(DeviceTwinBinding *deviceTwinBinding, char *reportedState), int requestId)
{
	JSON_Value *root_value = NULL;
	JSON_Object *root_object = NULL;

	_updateReportedStateCallback = updateReportedStateCallback;

	char *payLoadString = (char *)malloc(payloadSize + 1);
	if (payLoadString == NULL)
	{
		goto cleanup;
	}

	memcpy(payLoadString, payload, payloadSize);
	payLoadString[payloadSize] = 0; //null terminate string

	root_value = json_parse_string(payLoadString);
	if (root_value == NULL)
	{
		goto cleanup;
	}

	root_object = json_value_get_object(root_value);
	if (root_object == NULL)
	{
		goto cleanup;
	}

	JSON_Object *desiredProperties = json_object_dotget_object(root_object, "desired");
	if (desiredProperties == NULL)
	{
		desiredProperties = root_object;
	}

	for (int i = 0; i < _deviceTwinCount; i++)
	{
		JSON_Object *currentJSONProperties = json_object_dotget_object(desiredProperties, _deviceTwins[i]->twinProperty);
		if (currentJSONProperties != NULL)
		{
			SetDesiredState(currentJSONProperties, _deviceTwins[i], requestId);
		}
	}

cleanup:
	// Release the allocated memory.
	if (root_value != NULL)
	{
		json_value_free(root_value);
	}

	if (payLoadString != NULL)
	{
		free(payLoadString);
		payLoadString = NULL;
	}
}

/// <summary>
///     Checks to see if the device twin twinProperty(name) is found in the json object. If yes, then act upon the request
/// </summary>
void SetDesiredState(JSON_Object *jsonObject, DeviceTwinBinding *deviceTwinBinding, int requestId)
{
	if (deviceTwinBinding->twinType == TYPE_UNKNOWN)
	{
		return;
	}

	switch (deviceTwinBinding->twinType)
	{
	case TYPE_INT:
		if (json_object_has_value_of_type(jsonObject, "value", JSONNumber))
		{
			*(int *)deviceTwinBinding->twinState = (int)json_object_get_number(jsonObject, "value");

			if (deviceTwinBinding->handler != NULL)
			{
				deviceTwinBinding->handler(deviceTwinBinding);
			}
			DeviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState, requestId);
		}
		break;
	case TYPE_FLOAT:
		if (json_object_has_value_of_type(jsonObject, "value", JSONNumber))
		{
			*(float *)deviceTwinBinding->twinState = (float)json_object_get_number(jsonObject, "value");

			if (deviceTwinBinding->handler != NULL)
			{
				deviceTwinBinding->handler(deviceTwinBinding);
			}
			DeviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState, requestId);
		}
		break;
	case TYPE_BOOL:
		if (json_object_has_value_of_type(jsonObject, "value", JSONBoolean))
		{
			*(bool *)deviceTwinBinding->twinState = (bool)json_object_get_boolean(jsonObject, "value");

			if (deviceTwinBinding->handler != NULL)
			{
				deviceTwinBinding->handler(deviceTwinBinding);
			}
			DeviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState, requestId);
		}
		break;
	case TYPE_STRING:
		if (json_object_has_value_of_type(jsonObject, "value", JSONString))
		{
			deviceTwinBinding->twinState = (char *)json_object_get_string(jsonObject, "value");

			if (deviceTwinBinding->handler != NULL)
			{
				deviceTwinBinding->handler(deviceTwinBinding);
			}
			DeviceTwinReportState(deviceTwinBinding, deviceTwinBinding->twinState, requestId);
			deviceTwinBinding->twinState = NULL;
		}
		break;
	default:
		break;
	}
}

bool DeviceTwinReportState(DeviceTwinBinding *deviceTwinBinding, void *state, int requestId)
{
	int len = 0;
	size_t reportLen = 10; // initialize to 10 chars to allow for JSON and NULL termination. This is generous by a couple of bytes
	bool result = false;

	if (deviceTwinBinding == NULL)
	{
		return false;
	}

	if (deviceTwinBinding->twinType == TYPE_UNKNOWN)
	{
		return false;
	}

	deviceTwinBinding->requestId = requestId;

	reportLen += strlen(deviceTwinBinding->twinProperty); // allow for twin property name in JSON response

	if (deviceTwinBinding->twinType == TYPE_STRING)
	{
		reportLen += strlen((char *)state);
	}
	else
	{
		reportLen += 20; // allow 20 chars for Int, float, and boolean serialization
	}

	char *reportedPropertiesString = (char *)malloc(reportLen);
	if (reportedPropertiesString == NULL)
	{
		return false;
	}

	memset(reportedPropertiesString, 0, reportLen);

	switch (deviceTwinBinding->twinType)
	{
	case TYPE_INT:
		*(int *)deviceTwinBinding->twinState = *(int *)state;
		len = snprintf(reportedPropertiesString, reportLen, "{\"%s\":%d}", deviceTwinBinding->twinProperty,
					   (*(int *)deviceTwinBinding->twinState));
		break;
	case TYPE_FLOAT:
		*(float *)deviceTwinBinding->twinState = *(float *)state;
		len = snprintf(reportedPropertiesString, reportLen, "{\"%s\":%f}", deviceTwinBinding->twinProperty,
					   (*(float *)deviceTwinBinding->twinState));
		break;
	case TYPE_BOOL:
		*(bool *)deviceTwinBinding->twinState = *(bool *)state;
		len = snprintf(reportedPropertiesString, reportLen, "{\"%s\":%s}", deviceTwinBinding->twinProperty,
					   (*(bool *)deviceTwinBinding->twinState ? "true" : "false"));
		break;
	case TYPE_STRING:
		deviceTwinBinding->twinState = NULL;
		len = snprintf(reportedPropertiesString, reportLen, "{\"%s\":\"%s\"}", deviceTwinBinding->twinProperty, (char *)state);
		break;
	case TYPE_UNKNOWN:
		break;
	default:
		break;
	}

	if (len > 0)
	{
		// result = DeviceTwinUpdateReportedState(deviceTwinBinding, reportedPropertiesString);
		result = _updateReportedStateCallback(deviceTwinBinding, reportedPropertiesString);
	}

	if (reportedPropertiesString != NULL)
	{
		free(reportedPropertiesString);
		reportedPropertiesString = NULL;
	}

	return result;
}

/// <summary>
///     Callback invoked when the Device Twin reported properties are accepted by IoT Hub.
/// </summary>
// void DeviceTwinsReportStatusCallback(int result, void *context)
// {
// 	Log_Debug("INFO: Device Twin reported properties update result: HTTP status code %d\n", result);
// }
