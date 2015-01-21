#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__),
    os.pardir, os.pardir, os.pardir, 'tools', 'telemetry'))

from telemetry import benchmark_runner
from telemetry.core import environment


def _LaunchDBus():
  """Launches DBus to work around a bug in GLib.

  Works around a bug in GLib where it performs operations which aren't
  async-signal-safe (in particular, memory allocations) between fork and exec
  when it spawns subprocesses. This causes threads inside Chrome's browser and
  utility processes to get stuck, and this harness to hang waiting for those
  processes, which will never terminate. This doesn't happen on users'
  machines, because they have an active desktop session and the
  DBUS_SESSION_BUS_ADDRESS environment variable set, but it does happen on the
  bots. See crbug.com/309093 for more details.

  Returns:
    True if it actually spawned DBus.
  """
  import platform
  if (platform.uname()[0].lower() == 'linux' and
      'DBUS_SESSION_BUS_ADDRESS' not in os.environ):
    try:
      print 'DBUS_SESSION_BUS_ADDRESS env var not found, starting dbus-launch'
      dbus_output = subprocess.check_output(['dbus-launch']).split('\n')
      for line in dbus_output:
        m = re.match(r'([^=]+)\=(.+)', line)
        if m:
          os.environ[m.group(1)] = m.group(2)
          print ' setting %s to %s' % (m.group(1), m.group(2))
      return True
    except (subprocess.CalledProcessError, OSError) as e:
      print 'Exception while running dbus_launch: %s' % e
  return False


def _ShutdownDBus():
  """Manually kills the previously-launched DBus daemon.

  It appears that passing --exit-with-session to dbus-launch in
  _LaunchDBus(), above, doesn't cause the launched dbus-daemon to shut
  down properly. Manually kill the sub-process using the PID it gave
  us at launch time.

  This function is called when the flag --spawn-dbus is given, and if
  _LaunchDBus(), above, actually spawned the dbus-daemon.
  """
  import signal
  if 'DBUS_SESSION_BUS_PID' in os.environ:
    dbus_pid = os.environ['DBUS_SESSION_BUS_PID']
    try:
      os.kill(int(dbus_pid), signal.SIGTERM)
      print ' killed dbus-daemon with PID %s' % dbus_pid
    except OSError as e:
      print ' error killing dbus-daemon with PID %s: %s' % (dbus_pid, e)
  # Try to clean up any stray DBUS_SESSION_BUS_ADDRESS environment
  # variable too. Some of the bots seem to re-invoke runtest.py in a
  # way that this variable sticks around from run to run.
  if 'DBUS_SESSION_BUS_ADDRESS' in os.environ:
    del os.environ['DBUS_SESSION_BUS_ADDRESS']
    print ' cleared DBUS_SESSION_BUS_ADDRESS environment variable'


if __name__ == '__main__':
  base_dir = os.path.dirname(os.path.realpath(__file__))
  benchmark_runner.config = environment.Environment([base_dir])

  did_launch_dbus = _LaunchDBus()
  try:
    retcode = benchmark_runner.main()
  finally:
    if did_launch_dbus:
      _ShutdownDBus()

  sys.exit(retcode)
