#include "processjob.h"

#include <QDir>

#ifdef Q_OS_WIN

#include <QWinEventNotifier>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <vector>

namespace {

QString windowsErrorString(DWORD errorCode)
{
    wchar_t *buffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                          | FORMAT_MESSAGE_FROM_SYSTEM
                                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr,
                                      errorCode,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&buffer),
                                      0,
                                      nullptr);

    QString message;
    if (size > 0 && buffer) {
        message = QString::fromWCharArray(buffer, static_cast<int>(size)).trimmed();
    } else {
        message = QStringLiteral("Windows 错误码 %1").arg(errorCode);
    }

    if (buffer) {
        LocalFree(buffer);
    }

    return message;
}

QString quoteWindowsArgument(const QString &argument)
{
    if (argument.isEmpty()) {
        return QStringLiteral("\"\"");
    }

    bool needsQuotes = false;
    for (const QChar ch : argument) {
        if (ch.isSpace() || ch == QLatin1Char('"')) {
            needsQuotes = true;
            break;
        }
    }

    if (!needsQuotes) {
        return argument;
    }

    QString result = QStringLiteral("\"");
    int backslashCount = 0;
    for (const QChar ch : argument) {
        if (ch == QLatin1Char('\\')) {
            ++backslashCount;
            continue;
        }

        if (ch == QLatin1Char('"')) {
            result += QString(backslashCount * 2 + 1, QLatin1Char('\\'));
            result += ch;
            backslashCount = 0;
            continue;
        }

        if (backslashCount > 0) {
            result += QString(backslashCount, QLatin1Char('\\'));
            backslashCount = 0;
        }
        result += ch;
    }

    if (backslashCount > 0) {
        result += QString(backslashCount * 2, QLatin1Char('\\'));
    }
    result += QLatin1Char('"');
    return result;
}

QString makeCommandLine(const QString &program, const QStringList &arguments)
{
    QStringList parts;
    parts.append(quoteWindowsArgument(program));
    for (const QString &argument : arguments) {
        parts.append(quoteWindowsArgument(argument));
    }
    return parts.join(QLatin1Char(' '));
}

} // namespace

#else

#include <QProcess>

#endif

ProcessJob::ProcessJob(QObject *parent)
    : QObject(parent)
{
#ifndef Q_OS_WIN
    process_ = new QProcess(this);
    QObject::connect(process_,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     this,
                     [this](int exitCode, QProcess::ExitStatus) {
                         running_ = false;
                         if (finishedCallback_) {
                             finishedCallback_(exitCode);
                         }
                     });
#endif
}

ProcessJob::~ProcessJob()
{
    close();
}

bool ProcessJob::start(const QString &program,
                       const QStringList &arguments,
                       const QString &workingDirectory)
{
    close();
    lastErrorString_.clear();

#ifdef Q_OS_WIN
    if (!createJob()) {
        return false;
    }

    const QString commandLine = makeCommandLine(program, arguments);
    std::wstring commandLineString = commandLine.toStdWString();
    std::vector<wchar_t> commandLineBuffer(commandLineString.begin(), commandLineString.end());
    commandLineBuffer.push_back(L'\0');

    std::wstring workingDirectoryString = QDir::toNativeSeparators(workingDirectory).toStdWString();

    STARTUPINFOW startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(processInfo));

    const BOOL created = CreateProcessW(nullptr,
                                        commandLineBuffer.data(),
                                        nullptr,
                                        nullptr,
                                        FALSE,
                                        CREATE_SUSPENDED | CREATE_NO_WINDOW,
                                        nullptr,
                                        workingDirectoryString.empty() ? nullptr : workingDirectoryString.c_str(),
                                        &startupInfo,
                                        &processInfo);
    if (!created) {
        lastErrorString_ = windowsErrorString(GetLastError());
        close();
        return false;
    }

    const BOOL assigned = AssignProcessToJobObject(static_cast<HANDLE>(jobHandle_),
                                                   processInfo.hProcess);
    if (!assigned) {
        lastErrorString_ = windowsErrorString(GetLastError());
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        close();
        return false;
    }

    processHandle_ = processInfo.hProcess;
    processId_ = processInfo.dwProcessId;
    running_ = true;

    processFinishedNotifier_ = new QWinEventNotifier(static_cast<HANDLE>(processHandle_), this);
    QObject::connect(static_cast<QWinEventNotifier *>(processFinishedNotifier_),
                     &QWinEventNotifier::activated,
                     this,
                     [this]() {
                         onProcessFinished();
                     });

    const DWORD resumed = ResumeThread(processInfo.hThread);
    CloseHandle(processInfo.hThread);
    if (resumed == static_cast<DWORD>(-1)) {
        lastErrorString_ = windowsErrorString(GetLastError());
        close();
        return false;
    }

    return true;
#else
    process_->setWorkingDirectory(workingDirectory);
    process_->setProgram(program);
    process_->setArguments(arguments);
    process_->start();
    if (!process_->waitForStarted(3000)) {
        lastErrorString_ = process_->errorString();
        return false;
    }

    processId_ = static_cast<quint64>(process_->processId());
    running_ = true;
    return true;
#endif
}

void ProcessJob::close()
{
#ifdef Q_OS_WIN
    if (processFinishedNotifier_) {
        static_cast<QWinEventNotifier *>(processFinishedNotifier_)->setEnabled(false);
        delete processFinishedNotifier_;
        processFinishedNotifier_ = nullptr;
    }

    if (jobHandle_) {
        TerminateJobObject(static_cast<HANDLE>(jobHandle_), 1);
        CloseHandle(static_cast<HANDLE>(jobHandle_));
        jobHandle_ = nullptr;
    }

    closeProcessHandle();
    running_ = false;
    processId_ = 0;
#else
    if (process_ && process_->state() != QProcess::NotRunning) {
        process_->terminate();
    }
    running_ = process_ && process_->state() != QProcess::NotRunning;
    processId_ = running_ ? static_cast<quint64>(process_->processId()) : 0;
#endif
}

bool ProcessJob::isActive() const
{
#ifdef Q_OS_WIN
    return jobHandle_ != nullptr;
#else
    return process_ && process_->state() != QProcess::NotRunning;
#endif
}

quint64 ProcessJob::processId() const
{
    return processId_;
}

QString ProcessJob::lastErrorString() const
{
    return lastErrorString_;
}

void ProcessJob::setFinishedCallback(const std::function<void(int exitCode)> &callback)
{
    finishedCallback_ = callback;
}

#ifdef Q_OS_WIN

bool ProcessJob::createJob()
{
    jobHandle_ = CreateJobObjectW(nullptr, nullptr);
    if (!jobHandle_) {
        lastErrorString_ = windowsErrorString(GetLastError());
        return false;
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limitInfo;
    ZeroMemory(&limitInfo, sizeof(limitInfo));
    limitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    const BOOL configured = SetInformationJobObject(static_cast<HANDLE>(jobHandle_),
                                                    JobObjectExtendedLimitInformation,
                                                    &limitInfo,
                                                    sizeof(limitInfo));
    if (!configured) {
        lastErrorString_ = windowsErrorString(GetLastError());
        CloseHandle(static_cast<HANDLE>(jobHandle_));
        jobHandle_ = nullptr;
        return false;
    }

    return true;
}

void ProcessJob::closeProcessHandle()
{
    if (!processHandle_) {
        return;
    }

    CloseHandle(static_cast<HANDLE>(processHandle_));
    processHandle_ = nullptr;
}

void ProcessJob::onProcessFinished()
{
    if (processFinishedNotifier_) {
        static_cast<QWinEventNotifier *>(processFinishedNotifier_)->setEnabled(false);
        processFinishedNotifier_->deleteLater();
        processFinishedNotifier_ = nullptr;
    }

    DWORD exitCode = 0;
    if (processHandle_) {
        GetExitCodeProcess(static_cast<HANDLE>(processHandle_), &exitCode);
    }

    running_ = false;
    closeProcessHandle();

    if (finishedCallback_) {
        finishedCallback_(static_cast<int>(exitCode));
    }
}

#endif