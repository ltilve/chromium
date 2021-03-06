// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_H_
#define DEVICE_USB_USB_DEVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"

namespace device {

class UsbDeviceHandle;
struct UsbConfigDescriptor;

// A UsbDevice object represents a detected USB device, providing basic
// information about it. For further manipulation of the device, a
// UsbDeviceHandle must be created from Open() method.
class UsbDevice : public base::RefCountedThreadSafe<UsbDevice> {
 public:
  typedef base::Callback<void(bool success)> ResultCallback;

  // Accessors to basic information.
  uint16 vendor_id() const { return vendor_id_; }
  uint16 product_id() const { return product_id_; }
  uint32 unique_id() const { return unique_id_; }

  // On ChromeOS the permission_broker service is used to change the ownership
  // of USB device nodes so that Chrome can open them. On other platforms these
  // functions are no-ops and always return true.
  virtual void CheckUsbAccess(const ResultCallback& callback);

  // Like CheckUsbAccess but actually changes the ownership of the device node.
  virtual void RequestUsbAccess(int interface_id,
                                const ResultCallback& callback);

  // Creates a UsbDeviceHandle for further manipulation.
  // Blocking method. Must be called on FILE thread.
  virtual scoped_refptr<UsbDeviceHandle> Open() = 0;

  // Explicitly closes a device handle. This method will be automatically called
  // by the destructor of a UsbDeviceHandle as well.
  // Closing a closed handle is a safe
  // Blocking method. Must be called on FILE thread.
  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) = 0;

  // Gets the UsbConfigDescriptor for the active device configuration or nullptr
  // if the device is unconfigured.
  // Blocking method. Must be called on FILE thread.
  virtual const UsbConfigDescriptor* GetConfiguration() = 0;

  // Gets the manufacturer string of the device, or false and an empty
  // string. This is a blocking method and must be called on FILE thread.
  // TODO(reillyg): Make this available from the UI thread. crbug.com/427985
  virtual bool GetManufacturer(base::string16* manufacturer) = 0;

  // Gets the product string of the device, or returns false and an empty
  // string. This is a blocking method and must be called on FILE thread.
  // TODO(reillyg): Make this available from the UI thread. crbug.com/427985
  virtual bool GetProduct(base::string16* product) = 0;

  // Gets the serial number string of the device, or returns false and an empty
  // string. This is a blocking method and must be called on FILE thread.
  // TODO(reillyg): Make this available from the UI thread. crbug.com/427985
  virtual bool GetSerialNumber(base::string16* serial) = 0;

 protected:
  UsbDevice(uint16 vendor_id, uint16 product_id, uint32 unique_id);
  virtual ~UsbDevice();

 private:
  friend class base::RefCountedThreadSafe<UsbDevice>;

  const uint16 vendor_id_;
  const uint16 product_id_;
  const uint32 unique_id_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_H_
