/*
 * The Clear BSD License
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 * that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "usb_device_config.h"
#include "usb.h"
#include "usb_device.h"

#include "usb_device_class.h"
#include "usb_device_hid.h"
#include "usb_device_ch9.h"
#include "usb_device_descriptor.h"
#include "mouse.h"

#include "fsl_device_registers.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_debug_console.h"

#include <stdio.h>
#include <stdlib.h>
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
#include "fsl_sysmpu.h"
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
#include "usb_phy.h"
#endif

#include "pin_mux.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*STATE SIZE is the number of commands*/
#define STATE_SIZE 42

/*We define all the commands for the keyboard*/
#define KEY_A 0x04U
#define KEY_B 0x05U
#define KEY_C 0x06U
#define KEY_D 0x07U
#define KEY_E 0x08U
#define KEY_F 0x09U
#define KEY_G 0x0AU
#define KEY_H 0x0BU
#define KEY_I 0x0CU
#define KEY_J 0x0DU
#define KEY_K 0x0EU
#define KEY_L 0x0FU
#define KEY_M 0x10U
#define KEY_N 0x11U
#define KEY_O 0x12U
#define KEY_P 0x13U
#define KEY_Q 0x14U
#define KEY_R 0x15U
#define KEY_S 0x16U
#define KEY_T 0x17U
#define KEY_U 0x18U
#define KEY_V 0x19U
#define KEY_W 0x1AU
#define KEY_X 0x1BU
#define KEY_Y 0x1CU
#define KEY_Z 0x1DU
#define KEY_ENTER 0x28U
#define KEY_SPACE 0x2CU
#define KEY_START 0x08U
#define KEY_RIGHTARROW 0x4FU
#define KEY_LEFTARROW 0x50U
#define KEY_DOWNARROW 0x51U
#define KEY_UPARROW 0x52U
#define KEY_CTRL 0x01U
#define KEY_DELAY 50
#define ENTER_DELAY 250
#define WIN_DELAY 200
#define MOUSE_DELAY 150
#define KEYBOARD_ID 2
#define MOUSE_ID 1
/*This is our state machine with device we select between Mouse and Keyboard
 *if the device is selected as a Keyboard data 1, data2 and data 3 are taking from
 *Wireshark response, if the device is selected as a mouse data 1 is the right click
 *data 2 is the x axis and data 3 is the y axis.
 *time will be the time the command is press */
typedef struct _fsm
{
	uint8_t device;
	uint8_t data1;
	uint8_t data2;
	uint8_t data3;
	uint8_t time;
}fsm_t;

/*The next part of code is the routine specified in the document*/
fsm_t fsm[STATE_SIZE] =
{
	{KEYBOARD_ID,KEY_START,0,0,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_P,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_I,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_N,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_T,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_A,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_ENTER,ENTER_DELAY},
	{MOUSE_ID,0,0,2,MOUSE_DELAY},
	{MOUSE_ID,1,-2,0,MOUSE_DELAY},
	{MOUSE_ID,1,0,2,MOUSE_DELAY},
	{MOUSE_ID,1,2,0,MOUSE_DELAY},
	{MOUSE_ID,1,0,-2,MOUSE_DELAY},
	{MOUSE_ID,0,0,0,1},
	{KEYBOARD_ID,8,0,0,MOUSE_DELAY},
	{KEYBOARD_ID,0,0,KEY_G,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_E,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_D,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_I,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_T,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_ENTER,ENTER_DELAY},
	{KEYBOARD_ID,KEY_START + KEY_CTRL,0,KEY_LEFTARROW,150},
	{KEYBOARD_ID,KEY_CTRL,0,KEY_N,ENTER_DELAY},
	{MOUSE_ID,0,-2,0,MOUSE_DELAY},
	{MOUSE_ID,1,0,0,1},
	{MOUSE_ID,0,0,0,1},
	{KEYBOARD_ID,0,0,KEY_H,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_O,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_L,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_A,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_SPACE,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_M,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_U,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_N,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_D,KEY_DELAY},
	{KEYBOARD_ID,0,0,KEY_O,KEY_DELAY},
	{KEYBOARD_ID,KEY_CTRL,0,KEY_A,KEY_DELAY},
	{KEYBOARD_ID,KEY_CTRL,0,KEY_C,KEY_DELAY},
	{MOUSE_ID,0,2,0,MOUSE_DELAY},
	{MOUSE_ID,1,0,0,1},
	{MOUSE_ID,0,0,0,1},
	{KEYBOARD_ID,KEY_CTRL,0,KEY_V,KEY_DELAY},
	{KEYBOARD_ID,0,0,0,KEY_DELAY}
};
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_InitHardware(void);
void USB_DeviceClockInit(void);
void USB_DeviceIsrEnable(void);
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle);
#endif

static usb_status_t USB_DeviceHidMouseAction(void);
static usb_status_t USB_DeviceHidMouseCallback(class_handle_t handle, uint32_t event, void *param);
static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param);
static void USB_DeviceApplicationInit(void);

/*******************************************************************************
 * Variables
 ******************************************************************************/

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE) static uint8_t s_MouseBuffer[USB_HID_MOUSE_REPORT_LENGTH];
/*We create a new buffer for the keyboard from the size of the REPORT_LENGHT*/
static uint8_t s_KeyboardBuffer[USB_HID_KEYBOARD_REPORT_LENGTH];
usb_hid_mouse_struct_t g_UsbDeviceHidMouse;

extern usb_device_class_struct_t g_UsbDeviceHidMouseConfig;

#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
usb_device_dcd_charging_time_t g_UsbDeviceDcdTimingConfig;
#endif

/* Set class configurations */
usb_device_class_config_struct_t g_UsbDeviceHidConfig[1] = {{
    USB_DeviceHidMouseCallback, /* HID mouse class callback pointer */
    (class_handle_t)NULL,       /* The HID class handle, This field is set by USB_DeviceClassInit */
    &g_UsbDeviceHidMouseConfig, /* The HID mouse configuration, including class code, subcode, and protocol, class type,
                           transfer type, endpoint address, max packet size, etc.*/
}};

/* Set class configuration list */
usb_device_class_config_list_struct_t g_UsbDeviceHidConfigList = {
    g_UsbDeviceHidConfig, /* Class configurations */
    USB_DeviceCallback,   /* Device callback pointer */
    1U,                   /* Class count */
};

/*******************************************************************************
 * Code
 ******************************************************************************/
#if (defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U))
void USB0_IRQHandler(void)
{
    USB_DeviceKhciIsrFunction(g_UsbDeviceHidMouse.deviceHandle);
    /* Add for ARM errata 838869, affects Cortex-M4, Cortex-M4F Store immediate overlapping
    exception return operation might vector to incorrect interrupt */
    __DSB();
}
#endif
void USB_DeviceClockInit(void)
{
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    SystemCoreClockUpdate();
    CLOCK_EnableUsbfs0Clock(kCLOCK_UsbSrcIrc48M, 48000000U);
/*
 * If the SOC has USB KHCI dedicated RAM, the RAM memory needs to be clear after
 * the KHCI clock is enabled. When the demo uses USB EHCI IP, the USB KHCI dedicated
 * RAM can not be used and the memory can't be accessed.
 */
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM) && (FSL_FEATURE_USB_KHCI_USB_RAM > 0U))
#if (defined(FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS) && (FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS > 0U))
    for (int i = 0; i < FSL_FEATURE_USB_KHCI_USB_RAM; i++)
    {
        ((uint8_t *)FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS)[i] = 0x00U;
    }
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM_BASE_ADDRESS */
#endif /* FSL_FEATURE_USB_KHCI_USB_RAM */
#endif
}
void USB_DeviceIsrEnable(void)
{
    uint8_t irqNumber;
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    uint8_t usbDeviceKhciIrq[] = USB_IRQS;
    irqNumber = usbDeviceKhciIrq[CONTROLLER_ID - kUSB_ControllerKhci0];
#endif
/* Install isr, set priority, and enable IRQ. */
    NVIC_SetPriority((IRQn_Type)irqNumber, USB_DEVICE_INTERRUPT_PRIORITY);
    EnableIRQ((IRQn_Type)irqNumber);
}
#if USB_DEVICE_CONFIG_USE_TASK
void USB_DeviceTaskFn(void *deviceHandle)
{
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0U)
    USB_DeviceKhciTaskFunction(deviceHandle);
#endif
}
#endif

static uint8_t count = 0, currentState = 0;


static usb_status_t USB_DeviceHidMouseAction(void)
{
	uint8_t length;

	count++;
	/*We going to do this until time is equal to the count*/
	if(count == fsm[currentState].time)
	{
		count = 0;
		/*We change the length depending if we gonna send a command from the mouse
		 * o keyboard*/
		if(fsm[currentState].device == 1)
		{
			g_UsbDeviceHidMouse.buffer = s_MouseBuffer;
			length = USB_HID_MOUSE_REPORT_LENGTH;
		}
		else
		{
			g_UsbDeviceHidMouse.buffer = s_KeyboardBuffer;
			length = USB_HID_KEYBOARD_REPORT_LENGTH;
		}

		g_UsbDeviceHidMouse.buffer[0] = fsm[currentState].device;
		g_UsbDeviceHidMouse.buffer[1] = fsm[currentState].data1;
		g_UsbDeviceHidMouse.buffer[2] = fsm[currentState].data2;
		g_UsbDeviceHidMouse.buffer[3] = fsm[currentState].data3;
		/*We move to next command*/
		currentState++;

		if(currentState > STATE_SIZE)
			currentState = STATE_SIZE;
	}
	/*In case that count is not equal to buffer time we do the action again for the selected
	 * device*/
	else if(fsm[currentState].device == 1)
	{
		g_UsbDeviceHidMouse.buffer = s_MouseBuffer;
		length = USB_HID_MOUSE_REPORT_LENGTH;
		g_UsbDeviceHidMouse.buffer[0] = fsm[currentState].device;
		g_UsbDeviceHidMouse.buffer[1] = fsm[currentState].data1;
		g_UsbDeviceHidMouse.buffer[2] = fsm[currentState].data2;
		g_UsbDeviceHidMouse.buffer[3] = fsm[currentState].data3;
	}

	else
	{
		g_UsbDeviceHidMouse.buffer = s_KeyboardBuffer;
		length = USB_HID_KEYBOARD_REPORT_LENGTH;

		g_UsbDeviceHidMouse.buffer[0] = fsm[currentState].device;
		g_UsbDeviceHidMouse.buffer[1] = 0;
		g_UsbDeviceHidMouse.buffer[2] = 0;
		g_UsbDeviceHidMouse.buffer[3] = 0;
	}



    /* Send report to the host */
    return USB_DeviceHidSend(g_UsbDeviceHidMouse.hidHandle, USB_HID_MOUSE_ENDPOINT_IN,
			g_UsbDeviceHidMouse.buffer, length);;
}

/* The hid class callback */
static usb_status_t USB_DeviceHidMouseCallback(class_handle_t handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_Error;
    usb_device_endpoint_callback_message_struct_t *message = (usb_device_endpoint_callback_message_struct_t *)param;

    switch (event)
    {
        case kUSB_DeviceHidEventSendResponse:
            /* Resport sent */
            if (g_UsbDeviceHidMouse.attach)
            {
                if ((NULL != message) && (message->length == USB_UNINITIALIZED_VAL_32))
                {
                    return error;
                }
                error = USB_DeviceHidMouseAction();
            }
            break;
        case kUSB_DeviceHidEventGetReport:
        case kUSB_DeviceHidEventSetReport:
        case kUSB_DeviceHidEventRequestReportBuffer:
            error = kStatus_USB_InvalidRequest;
            break;
        case kUSB_DeviceHidEventGetIdle:
        case kUSB_DeviceHidEventGetProtocol:
        case kUSB_DeviceHidEventSetIdle:
        case kUSB_DeviceHidEventSetProtocol:
            break;
        default:
            break;
    }

    return error;
}

/* The device callback */
static usb_status_t USB_DeviceCallback(usb_device_handle handle, uint32_t event, void *param)
{
    usb_status_t error = kStatus_USB_Error;
    uint16_t *temp16 = (uint16_t *)param;
    uint8_t *temp8 = (uint8_t *)param;

    switch (event)
    {
        case kUSB_DeviceEventBusReset:
        {
            /* USB bus reset signal detected */
            g_UsbDeviceHidMouse.attach = 0U;
            error = kStatus_USB_Success;
#if (defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)) || \
    (defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U))
            /* Get USB speed to configure the device, including max packet size and interval of the endpoints. */
            if (kStatus_USB_Success == USB_DeviceClassGetSpeed(CONTROLLER_ID, &g_UsbDeviceHidMouse.speed))
            {
                USB_DeviceSetSpeed(handle, g_UsbDeviceHidMouse.speed);
            }
#endif
        }
        break;
#if (defined(USB_DEVICE_CONFIG_DETACH_ENABLE) && (USB_DEVICE_CONFIG_DETACH_ENABLE > 0U))
        case kUSB_DeviceEventAttach:
        {
#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
            g_UsbDeviceHidMouse.vReginInterruptDetected = 1;
            g_UsbDeviceHidMouse.vbusValid = 1;
#else
            usb_echo("USB device attached.\r\n");
            USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
#endif
        }
        break;
        case kUSB_DeviceEventDetach:
        {
#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
            g_UsbDeviceHidMouse.vReginInterruptDetected = 1;
            g_UsbDeviceHidMouse.vbusValid = 0;
            g_UsbDeviceHidMouse.attach = 0;
#else
            usb_echo("USB device detached.\r\n");
            g_UsbDeviceHidMouse.attach = 0;
            USB_DeviceStop(g_UsbDeviceHidMouse.deviceHandle);
#endif
        }
        break;
#endif
        case kUSB_DeviceEventSetConfiguration:
            if (param)
            {
                /* Set device configuration request */
                g_UsbDeviceHidMouse.attach = 1U;
                g_UsbDeviceHidMouse.currentConfiguration = *temp8;
                if (USB_HID_MOUSE_CONFIGURE_INDEX == (*temp8))
                {
                    error = USB_DeviceHidMouseAction();
                }
            }
            break;
        case kUSB_DeviceEventSetInterface:
            if (g_UsbDeviceHidMouse.attach)
            {
                /* Set device interface request */
                uint8_t interface = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                uint8_t alternateSetting = (uint8_t)(*temp16 & 0x00FFU);
                if (interface < USB_HID_MOUSE_INTERFACE_COUNT)
                {
                    g_UsbDeviceHidMouse.currentInterfaceAlternateSetting[interface] = alternateSetting;
                    if (alternateSetting == 0U)
                    {
                        error = USB_DeviceHidMouseAction();
                    }
                }
            }
            break;
        case kUSB_DeviceEventGetConfiguration:
            if (param)
            {
                /* Get current configuration request */
                *temp8 = g_UsbDeviceHidMouse.currentConfiguration;
                error = kStatus_USB_Success;
            }
            break;
        case kUSB_DeviceEventGetInterface:
            if (param)
            {
                /* Get current alternate setting of the interface request */
                uint8_t interface = (uint8_t)((*temp16 & 0xFF00U) >> 0x08U);
                if (interface < USB_HID_MOUSE_INTERFACE_COUNT)
                {
                    *temp16 = (*temp16 & 0xFF00U) | g_UsbDeviceHidMouse.currentInterfaceAlternateSetting[interface];
                    error = kStatus_USB_Success;
                }
                else
                {
                    error = kStatus_USB_InvalidRequest;
                }
            }
            break;
        case kUSB_DeviceEventGetDeviceDescriptor:
            if (param)
            {
                /* Get device descriptor request */
                error = USB_DeviceGetDeviceDescriptor(handle, (usb_device_get_device_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetConfigurationDescriptor:
            if (param)
            {
                /* Get device configuration descriptor request */
                error = USB_DeviceGetConfigurationDescriptor(handle,
                                                             (usb_device_get_configuration_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetStringDescriptor:
            if (param)
            {
                /* Get device string descriptor request */
                error = USB_DeviceGetStringDescriptor(handle, (usb_device_get_string_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetHidDescriptor:
            if (param)
            {
                /* Get hid descriptor request */
                error = USB_DeviceGetHidDescriptor(handle, (usb_device_get_hid_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetHidReportDescriptor:
            if (param)
            {
                /* Get hid report descriptor request */
                error =
                    USB_DeviceGetHidReportDescriptor(handle, (usb_device_get_hid_report_descriptor_struct_t *)param);
            }
            break;
        case kUSB_DeviceEventGetHidPhysicalDescriptor:
            if (param)
            {
                /* Get hid physical descriptor request */
                error = USB_DeviceGetHidPhysicalDescriptor(handle,
                                                           (usb_device_get_hid_physical_descriptor_struct_t *)param);
            }
            break;
#if (defined(USB_DEVICE_CONFIG_CV_TEST) && (USB_DEVICE_CONFIG_CV_TEST > 0U))
        case kUSB_DeviceEventGetDeviceQualifierDescriptor:
            if (param)
            {
                /* Get device descriptor request */
                error = USB_DeviceGetDeviceQualifierDescriptor(
                    handle, (usb_device_get_device_qualifier_descriptor_struct_t *)param);
            }
            break;
#endif
#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
        case kUSB_DeviceEventDcdTimeOut:
            if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusVBUSDetect)
            {
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusTimeOut;
            }
            break;
        case kUSB_DeviceEventDcdUnknownType:
            if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusVBUSDetect)
            {
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusUnknownType;
            }
            break;
        case kUSB_DeviceEventSDPDetected:
            if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusVBUSDetect)
            {
                g_UsbDeviceHidMouse.dcdPortType = kUSB_DeviceDCDPortTypeSDP;
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusDetectFinish;
            }
            break;
        case kUSB_DeviceEventChargingPortDetected:
            if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusVBUSDetect)
            {
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusChargingPortDetect;
            }
            break;
        case kUSB_DeviceEventChargingHostDetected:
            if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusVBUSDetect)
            {
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusDetectFinish;
                g_UsbDeviceHidMouse.dcdPortType = kUSB_DeviceDCDPortTypeCDP;
            }
            break;
        case kUSB_DeviceEventDedicatedChargerDetected:
            if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusVBUSDetect)
            {
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusDetectFinish;
                g_UsbDeviceHidMouse.dcdPortType = kUSB_DeviceDCDPortTypeDCP;
            }
            break;
#endif
        default:
            break;
    }

    return error;
}

static void USB_DeviceApplicationInit(void)
{
    USB_DeviceClockInit();
#if (defined(FSL_FEATURE_SOC_SYSMPU_COUNT) && (FSL_FEATURE_SOC_SYSMPU_COUNT > 0U))
    SYSMPU_Enable(SYSMPU, 0);
#endif /* FSL_FEATURE_SOC_SYSMPU_COUNT */

    /* Set HID mouse to default state */
    g_UsbDeviceHidMouse.speed = USB_SPEED_FULL;
    g_UsbDeviceHidMouse.attach = 0U;
    g_UsbDeviceHidMouse.hidHandle = (class_handle_t)NULL;
    g_UsbDeviceHidMouse.deviceHandle = NULL;
    g_UsbDeviceHidMouse.buffer = s_MouseBuffer;

#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
    g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusDetached;

    g_UsbDeviceDcdTimingConfig.dcdSeqInitTime = USB_DEVICE_DCD_SEQ_INIT_TIME;
    g_UsbDeviceDcdTimingConfig.dcdDbncTime = USB_DEVICE_DCD_DBNC_MSEC;
    g_UsbDeviceDcdTimingConfig.dcdDpSrcOnTime = USB_DEVICE_DCD_VDPSRC_ON_MSEC;
    g_UsbDeviceDcdTimingConfig.dcdTimeWaitAfterPrD = USB_DEVICE_DCD_TIME_WAIT_AFTER_PRI_DETECTION;
    g_UsbDeviceDcdTimingConfig.dcdTimeDMSrcOn = USB_DEVICE_DCD_TIME_DM_SRC_ON;
#endif

    /* Initialize the usb stack and class drivers */
    if (kStatus_USB_Success !=
        USB_DeviceClassInit(CONTROLLER_ID, &g_UsbDeviceHidConfigList, &g_UsbDeviceHidMouse.deviceHandle))
    {
        usb_echo("USB device mouse failed\r\n");
        return;
    }
    else
    {
#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
        usb_echo("USB device DCD + HID mouse demo\r\n");
#else
        usb_echo("USB device HID mouse demo\r\n");
#endif
        /* Get the HID mouse class handle */
        g_UsbDeviceHidMouse.hidHandle = g_UsbDeviceHidConfigList.config->classHandle;
    }

    USB_DeviceIsrEnable();

#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
#else
    /* Start USB device HID mouse */
    USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
#endif
}

#if defined(USB_DEVICE_CONFIG_USE_TASK) && (USB_DEVICE_CONFIG_USE_TASK > 0)
void USB_DeviceTask(void *handle)
{
    while (1U)
    {
        USB_DeviceTaskFn(handle);
    }
}
#endif

void APP_task(void *handle)
{
    USB_DeviceApplicationInit();

#if USB_DEVICE_CONFIG_USE_TASK
    if (g_UsbDeviceHidMouse.deviceHandle)
    {
        if (xTaskCreate(USB_DeviceTask,                       /* pointer to the task */
                        "usb device task",                    /* task name for kernel awareness debugging */
                        5000L / sizeof(portSTACK_TYPE),       /* task stack size */
                        g_UsbDeviceHidMouse.deviceHandle,     /* optional task startup argument */
                        5U,                                   /* initial priority */
                        &g_UsbDeviceHidMouse.deviceTaskHandle /* optional task handle to create */
                        ) != pdPASS)
        {
            usb_echo("usb device task create failed!\r\n");
            return;
        }
    }
#endif

    while (1U)
    {
#if (defined(USB_DEVICE_CHARGER_DETECT_ENABLE) && (USB_DEVICE_CHARGER_DETECT_ENABLE > 0U)) && \
    ((defined(FSL_FEATURE_SOC_USBDCD_COUNT) && (FSL_FEATURE_SOC_USBDCD_COUNT > 0U)) ||        \
     (defined(FSL_FEATURE_SOC_USBHSDCD_COUNT) && (FSL_FEATURE_SOC_USBHSDCD_COUNT > 0U)))
        if (g_UsbDeviceHidMouse.vReginInterruptDetected)
        {
            g_UsbDeviceHidMouse.vReginInterruptDetected = 0;
            if (g_UsbDeviceHidMouse.vbusValid)
            {
                usb_echo("USB device attached.\r\n");
                USB_DeviceDcdInitModule(g_UsbDeviceHidMouse.deviceHandle, &g_UsbDeviceDcdTimingConfig);
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusVBUSDetect;
            }
            else
            {
                usb_echo("USB device detached.\r\n");
                USB_DeviceDcdDeinitModule(g_UsbDeviceHidMouse.deviceHandle);
                USB_DeviceStop(g_UsbDeviceHidMouse.deviceHandle);
                g_UsbDeviceHidMouse.dcdPortType = kUSB_DeviceDCDPortTypeNoPort;
                g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusDetached;
            }
        }

        if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusChargingPortDetect) /* This is only for BC1.1 */
        {
            USB_DeviceRun(g_UsbDeviceHidMouse.deviceHandle);
        }
        if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusTimeOut)
        {
            usb_echo("Timeout error.\r\n");
            g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusComplete;
        }
        if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusUnknownType)
        {
            usb_echo("Unknown port type.\r\n");
            g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusComplete;
        }
        if (g_UsbDeviceHidMouse.dcdDevStatus == kUSB_DeviceDCDDevStatusDetectFinish)
        {
            if (g_UsbDeviceHidMouse.dcdPortType == kUSB_DeviceDCDPortTypeSDP)
            {
                usb_echo("The device has been connected to a facility which is SDP(Standard Downstream Port).\r\n");
                USB_DeviceRun(
                    g_UsbDeviceHidMouse.deviceHandle); /* If the facility attached is SDP, start enumeration */
            }
            else if (g_UsbDeviceHidMouse.dcdPortType == kUSB_DeviceDCDPortTypeCDP)
            {
                usb_echo("The device has been connected to a facility which is CDP(Charging Downstream Port).\r\n");
                USB_DeviceRun(
                    g_UsbDeviceHidMouse.deviceHandle); /* If the facility attached is CDP, start enumeration */
            }
            else if (g_UsbDeviceHidMouse.dcdPortType == kUSB_DeviceDCDPortTypeDCP)
            {
                usb_echo("The device has been connected to a facility which is DCP(Dedicated Charging Port).\r\n");
            }
            g_UsbDeviceHidMouse.dcdDevStatus = kUSB_DeviceDCDDevStatusComplete;
        }
#endif
    }
}

#if defined(__CC_ARM) || defined(__GNUC__)
int main(void)
#else
void main(void)
#endif
{
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    if (xTaskCreate(APP_task,                                  /* pointer to the task */
                    "app task",                                /* task name for kernel awareness debugging */
                    5000L / sizeof(portSTACK_TYPE),            /* task stack size */
                    &g_UsbDeviceHidMouse,                      /* optional task startup argument */
                    4U,                                        /* initial priority */
                    &g_UsbDeviceHidMouse.applicationTaskHandle /* optional task handle to create */
                    ) != pdPASS)
    {
        usb_echo("app task create failed!\r\n");
#if (defined(__CC_ARM) || defined(__GNUC__))
        return 1U;
#else
        return;
#endif
    }

    vTaskStartScheduler();

#if (defined(__CC_ARM) || defined(__GNUC__))
    return 1U;
#endif
}
