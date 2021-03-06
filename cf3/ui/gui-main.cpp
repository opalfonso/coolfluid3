// Copyright (C) 2010-2011 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <iostream>

#include <QApplication>

#include "common/CF.hpp"
#include "common/PE/Comm.hpp"
#include "common/Core.hpp"
#include "common/Environment.hpp"
#include "common/NetworkInfo.hpp"
#include "common/Exception.hpp"
#include "common/OptionList.hpp"

#include "ui/core/ThreadManager.hpp"

#include "ui/graphics/Application.hpp"
#include "ui/graphics/MainWindow.hpp"
#include "ui/graphics/JournalBrowserDialog.hpp"

using namespace cf3::common;
using namespace cf3::Tools::Shell;
using namespace cf3::ui::core;
using namespace cf3::ui::graphics;

int main(int argc, char *argv[])
{
  Application app(argc, argv);
  int returnValue;

  JournalBrowserBuilder::instance();

  ThreadManager::instance().tree();

  // initiate the core environment
  Core::instance().environment().options().set("regist_signal_handlers", false);
  Core::instance().initiate(argc, argv);
  PE::Comm::instance().init(argc,argv);   // this might modify argc and argv

  cf3::common::AssertionManager::instance().AssertionThrows = true;
  cf3::common::AssertionManager::instance().AssertionDumps = true;
  cf3::common::ExceptionManager::instance().ExceptionDumps = true;

  // tell CF core that the client is running
  Core::instance().network_info().start_client();

  try
  {
   MainWindow window;
   window.showMaximized();
   returnValue = app.exec();
  }
  catch(Exception e)
  {
    std::cerr << "Application stopped on uncaught exception:" << std::endl;
    std::cerr << e.what() << std::endl;
    returnValue = -1;
  }

  // tell CF core that the client is about to exit
  Core::instance().network_info().stop_client();

  // terminate the MPI environment

  PE::Comm::instance().finalize();
  Core::instance().terminate();

  return returnValue;
}
