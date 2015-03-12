#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Dump Chrome's ATK accessibility tree to the command line.

Accerciser is slow and buggy. This is a quick way to check that Chrome is
exposing its interface to ATK from the command line.
"""

import pyatspi

def Dump(obj, indent):
  if not obj:
    return
  indent_str = '  ' * indent
  role = obj.get_role_name()
  name = obj.get_name()
  print '%s%s name="%s"' % (indent_str, role, name)

  # Don't recurse into applications other than Chrome
  if role == 'application':
    if (name.lower().find('chrom') != 0 and
        name.lower().find('google chrome') != 0):
      return

  for i in range(obj.get_child_count()):
    Dump(obj.get_child_at_index(i), indent + 1)

desktop = pyatspi.Registry.getDesktop(0)
Dump(desktop, 0)
