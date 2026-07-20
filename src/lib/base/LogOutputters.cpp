/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "base/LogOutputters.h"
#include "arch/Arch.h"

#include <iostream>

#include <QFile>
#include <QString>
#include <QTextStream>

constexpr auto s_logFileSizeLimit = 1024 * 1024; //!< Max Log size before rotating (1Mb)

//
// StopLogOutputter
//

void StopLogOutputter::open(const QString &)
{
  // do nothing
}

void StopLogOutputter::close()
{
  // do nothing
}

bool StopLogOutputter::write(LogLevel::Level, const QString &)
{
  return false;
}

//
// ConsoleLogOutputter
//

void ConsoleLogOutputter::open(const QString &title)
{
  // do nothing
}

void ConsoleLogOutputter::close()
{
  // do nothing
}

bool ConsoleLogOutputter::write(LogLevel::Level level, const QString &msg)
{
  using enum LogLevel::Level;
  if ((level >= Fatal) && (level <= Warning))
    std::cerr << qPrintable(msg) << std::endl;
  else
    std::cout << qPrintable(msg) << std::endl;
  std::cout.flush();
  return true;
}

void ConsoleLogOutputter::flush() const
{
  // do nothing
}

//
// SystemLogOutputter
//

void SystemLogOutputter::open(const QString &title)
{
  ARCH->openLog(title);
}

void SystemLogOutputter::close()
{
  ARCH->closeLog();
}

bool SystemLogOutputter::write(LogLevel::Level level, const QString &msg)
{
  ARCH->writeLog(level, msg);
  return true;
}

//
// SystemLogger
//

SystemLogger::SystemLogger(const QString &title, bool blockConsole)
{
  // redirect log messages
  if (blockConsole) {
    m_stop = new StopLogOutputter; // NOSONAR - Adopted by `Log`
    CLOG->insert(m_stop);
  }
  m_syslog = new SystemLogOutputter; // NOSONAR - Adopted by `Log`
  m_syslog->open(title);
  CLOG->insert(m_syslog);
}

SystemLogger::~SystemLogger()
{
  CLOG->remove(m_syslog);
  delete m_syslog;
  if (m_stop != nullptr) {
    CLOG->remove(m_stop);
    delete m_stop;
  }
}

//
// FileLogOutputter
//

FileLogOutputter::FileLogOutputter(const QString &logFile)
{
  setLogFilename(logFile);
}

void FileLogOutputter::setLogFilename(const QString &logFile)
{
  assert(logFile != nullptr);
  if (m_file.isOpen())
    m_file.close();
  m_fileName = logFile;
}

bool FileLogOutputter::write(LogLevel::Level, const QString &message)
{
  // keep the file open across writes.  re-opening (and closing) it on every
  // single line is pathologically slow at high log levels and can stall the
  // thread long enough to trip connection/handshake timeouts.  writes are
  // already serialized by the logger's mutex, so a shared handle is safe.
  if (!m_file.isOpen()) {
    m_file.setFileName(m_fileName);
    if (!m_file.open(QFile::WriteOnly | QFile::Append | QFile::Text))
      return false;
  }

  {
    QTextStream stream(&m_file);
    stream << message << Qt::endl; // endl flushes so tail -f sees each line
  }

  if (m_file.size() > s_logFileSizeLimit) {
    // rotate: keep a single backup.  the previous implementation removed the
    // current file *before* renaming it, so the rename always failed and the
    // log was simply wiped with no backup kept.
    m_file.close();
    const auto oldFile = QStringLiteral("%1.1").arg(m_fileName);
    QFile::remove(oldFile);
    QFile::rename(m_fileName, oldFile);
    // next write reopens a fresh m_fileName
  }

  return true;
}

void FileLogOutputter::open(const QString &title)
{
  // do nothing
}

void FileLogOutputter::close()
{
  if (m_file.isOpen())
    m_file.close();
}
