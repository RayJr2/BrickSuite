/*
 * BrickSuite - The Digital Twin Platform for Your Brick Workshop
 *
 * Copyright (C) 2026 RF StateSide, LLC
 *
 * This file is part of BrickSuite.
 *
 * BrickSuite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 3 of the License.
 *
 * BrickSuite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with BrickSuite. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QElapsedTimer>
#include <QProcess>
#include <algorithm>
#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
#else
#include <sys/resource.h>
#endif

namespace PrintGeometry::BoundedGeometryWorker {
constexpr quint64 MemoryBudgetBytes=512ULL*1024*1024;
enum class Outcome { Finished, StartFailure, TimedOut, MemoryLimit, MonitorFailure };

// Called before reading operands. macOS workers cannot proceed until their
// parent has successfully sampled them and enabled continuous supervision.
inline bool constrainChild()
{
#ifdef Q_OS_WIN
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    const HANDLE job=CreateJobObjectW(nullptr,nullptr);
    if(!job)return false;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit=MemoryBudgetBytes;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))||
       !AssignProcessToJobObject(job,GetCurrentProcess())){CloseHandle(job);return false;}
    return true; // Keep the job handle alive until process exit.
#elif defined(Q_OS_MACOS)
    char permission=0;
    return ::read(STDIN_FILENO,&permission,1)==1&&permission=='G';
#else
    rlimit limit{};
    if(getrlimit(RLIMIT_AS,&limit)!=0)return false;
    limit.rlim_cur=std::min<rlim_t>(limit.rlim_cur,MemoryBudgetBytes);
    return setrlimit(RLIMIT_AS,&limit)==0;
#endif
}

// Own the entire start/wait/kill lifecycle. A lower budget is supported for
// controlled transport tests; callers cannot raise the production ceiling.
// The macOS policy is sampled, so transient overshoot between samples is possible.
inline Outcome run(QProcess& process,int deadlineMilliseconds,quint64 budget=MemoryBudgetBytes)
{
    const auto stop=[&](Outcome outcome){
        if(process.state()!=QProcess::NotRunning){process.kill();process.waitForFinished(3000);}
        return outcome;
    };
    process.start();
    if(!process.waitForStarted(3000))return stop(Outcome::StartFailure);
    QElapsedTimer timer;timer.start();
#ifdef Q_OS_MACOS
    budget=std::clamp<quint64>(budget,1,MemoryBudgetBytes);
    bool authorized=false;
    while(process.state()!=QProcess::NotRunning){
        rusage_info_v4 usage{};
        if(proc_pid_rusage(int(process.processId()),RUSAGE_INFO_V4,reinterpret_cast<rusage_info_t*>(&usage))!=0){
            // A child may have exited between QProcess state and the sample.
            // If it is still running, inability to monitor must fail closed.
            if(process.waitForFinished(1))break;
            return stop(Outcome::MonitorFailure);
        }
        const auto bytes=std::max({usage.ri_resident_size,usage.ri_phys_footprint,usage.ri_lifetime_max_phys_footprint});
        if(bytes>budget)return stop(Outcome::MemoryLimit);
        if(!authorized){
            if(process.write("G",1)!=1||!process.waitForBytesWritten(1000))return stop(Outcome::MonitorFailure);
            process.closeWriteChannel();authorized=true;
        }
        const auto remaining=deadlineMilliseconds-timer.elapsed();
        if(remaining<=0)return stop(Outcome::TimedOut);
        if(process.waitForFinished(int(std::min<qint64>(10,remaining))))break;
    }
#else
    Q_UNUSED(budget);
    if(!process.waitForFinished(deadlineMilliseconds))return stop(Outcome::TimedOut);
#endif
    return Outcome::Finished;
}
} // namespace PrintGeometry::BoundedGeometryWorker
