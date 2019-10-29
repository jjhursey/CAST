/*******************************************************************************
 |    TagFile.cc
 |
 |   Copyright IBM Corporation 2015,2016. All Rights Reserved
 |
 |    This program is licensed under the terms of the Eclipse Public License
 |    v1.0 as published by the Eclipse Foundation and available at
 |    http://www.eclipse.org/legal/epl-v10.html
 |
 |    U.S. Government Users Restricted Rights:  Use, duplication or disclosure
 |    restricted by GSA ADP Schedule Contract with IBM Corp.
 *******************************************************************************/

#include "bbserver_flightlog.h"
#include "TagFile.h"
#include "tracksyscall.h"

using namespace boost::archive;
namespace bfs = boost::filesystem;


/*******************************************************************************
 | External data
 *******************************************************************************/
thread_local int tagFileLockFd = -1;


/*
 * Static methods
 */

int TagFile::addTagHandle(const LVKey* pLVKey, const BBJob pJob, const uint64_t pTag, vector<uint32_t> pExpectContrib, uint64_t& pHandle)
{
    int rc = 0;
    stringstream errorText;

    TagFile* l_TagFile = 0;
    int l_TagFileLocked = 0;

    int l_LocalMetadataUnlocked = unlockLocalMetadataIfNeeded(pLVKey, "addTagHandle");

    try
    {
        bfs::path l_JobStepPath(config.get("bb.bbserverMetadataPath", DEFAULT_BBSERVER_METADATAPATH));
        l_JobStepPath /= bfs::path(to_string(pJob.getJobId()));
        l_JobStepPath /= bfs::path(to_string(pJob.getJobStepId()));
        if(!bfs::is_directory(l_JobStepPath))
        {
            rc = -1;
            errorText << "BBTagInfo::update_xbbServerAddData(): Attempt to create the tagfile failed because the jobstep directory " << l_JobStepPath.string() << " could not be found";
            LOG_ERROR_TEXT_RC_AND_BAIL(errorText, rc);
        }

        rc = TagFile::lock(l_JobStepPath);
        if (!rc)
        {
            l_TagFileLocked = 1;
            bfs::path l_TagFilePath = l_JobStepPath / TAGFILENAME;
            rc = TagFile::load(l_TagFile, l_TagFilePath);
            if (!rc)
            {
                for (size_t i=0; (i<l_TagFile->tagHandles.size() && (!rc)); i++)
                {
                    if (l_TagFile->tagHandles[i].handle == pHandle)
                    {
                        // Same handle value
                        if (l_TagFile->tagHandles[i].tag == pTag)
                        {
                            // Same tag value
                            if (!(l_TagFile->tagHandles[i].compareContrib(pExpectContrib)))
                            {
                                // Same contrib vector
                                rc = 1;
                            }
                            else
                            {
                                // Different contrib vector
                                rc = -3;
                            }
                        }
                        else
                        {
                            rc = 2;
                        }
                    }
                    else if (l_TagFile->tagHandles[i].tag == pTag)
                    {
                        // Different handle, same tag.  Error condition.
                        rc = -2;
                    }
                    else
                    {
                        // Neither the handle value nor the tag value match.  Continue...
                    }
                }

                if (!rc)
                {
                    BBTagHandle l_TagHandle = BBTagHandle(pTag, pHandle, pExpectContrib);
                    l_TagFile->tagHandles.push_back(l_TagHandle);
                    l_TagFile->save();
                }
            }
            else
            {
                // Could not load the tagfile
            }
        }
    }
    catch(ExceptionBailout& e) { }
    catch(exception& e)
    {
        rc = -1;
        LOG_ERROR_RC_WITH_EXCEPTION(__FILE__, __FUNCTION__, __LINE__, e, rc);
    }

    if (l_TagFileLocked)
    {
        l_TagFileLocked = 0;
        l_TagFile->unlock();
    }

    if (l_LocalMetadataUnlocked)
    {
        lockLocalMetadata(pLVKey, "addTagHandle");
    }

    return rc;
}

int TagFile::createLockFile(const string pFilePath)
{
    int rc = 0;

    char l_LockFileName[PATH_MAX+64] = {'\0'};
    snprintf(l_LockFileName, sizeof(l_LockFileName), "%s/%s", pFilePath.c_str(), LOCK_TAG_FILENAME);

    bfs::ofstream l_LockFile{l_LockFileName};

    return rc;
}

int TagFile::load(TagFile* &pTagFile, const bfs::path& pTagFileName)
{
    int rc;

    uint64_t l_FL_Counter = metadataCounter.getNext();
    FL_Write(FLMetaData, TF_Load, "loadTagFile, counter=%ld", l_FL_Counter, 0, 0, 0);

    pTagFile = NULL;
    TagFile* l_TagFile = new TagFile();

    struct timeval l_StartTime = timeval {.tv_sec=0, .tv_usec=0}, l_StopTime = timeval {.tv_sec=0, .tv_usec=0};
    bool l_AllDone = false;
    int l_Attempts = 0;
    int l_ElapsedTime = 0;
    int l_LastConsoleOutput = -1;

    while ((!l_AllDone) && (l_ElapsedTime < MAXIMUM_TAGFILE_LOADTIME))
    {
        rc = 0;
        l_AllDone = true;
        ++l_Attempts;
        try
        {
            LOG(bb,debug) << "Reading:" << pTagFileName;
            ifstream l_ArchiveFile{pTagFileName.c_str()};
            text_iarchive l_Archive{l_ArchiveFile};
            l_Archive >> *l_TagFile;
            pTagFile = l_TagFile;
        }
        catch(archive_exception& e)
        {
            // NOTE: If we take an 'archieve exception' we do not delay before attempting the next
            //       read of the archive file.  More than likely, we just had a concurrent update
            //       to the tagfile.
            rc = -1;
            l_AllDone = false;

            gettimeofday(&l_StopTime, NULL);
            if (l_Attempts == 1)
            {
                l_StartTime = l_StopTime;
            }
            l_ElapsedTime = int(l_StopTime.tv_sec - l_StartTime.tv_sec);

            if (l_ElapsedTime && (l_ElapsedTime % 3 == 0) && (l_ElapsedTime != l_LastConsoleOutput))
            {
                l_LastConsoleOutput = l_ElapsedTime;
                LOG(bb,warning) << "Archive exception thrown in " << __func__ << " was " << e.what() \
                                << " when attempting to load archive " << pTagFileName.c_str() << ". Elapsed time=" << l_ElapsedTime << " second(s). Retrying...";
            }
        }
        catch(exception& e)
        {
            rc = -1;
            LOG(bb,error) << "Exception thrown in " << __func__ << " was " << e.what() << " when attempting to load archive " << pTagFileName.c_str();
        }
    }

    if (l_LastConsoleOutput > 0)
    {
       gettimeofday(&l_StopTime, NULL);
       if (!rc)
        {
            LOG(bb,warning) << "Loading " << pTagFileName.c_str() << " became successful after " << (l_StopTime.tv_sec - l_StartTime.tv_sec) << " second(s)" << " after recovering from archive exception(s)";
        }
        else
        {
            LOG(bb,error) << "Loading " << pTagFileName.c_str() << " failed after " << (l_StopTime.tv_sec - l_StartTime.tv_sec) << " second(s)" << " when attempting to recover from archive exception(s)";
        }
    }

    if (rc)
    {
        if (l_TagFile)
        {
            delete l_TagFile;
            l_TagFile = NULL;
        }
    }

    FL_Write(FLMetaData, TF_Load_End, "loadTagFile, counter=%ld, rc=%ld", l_FL_Counter, rc, 0, 0);

    return rc;
}

int TagFile::lock(const bfs::path& pJobStepPath)
{
    int rc = -2;
    int fd = -1;
    stringstream errorText;

    std::string l_TagLockFileName = (pJobStepPath / LOCK_TAG_FILENAME).string();
    if (tagFileLockFd == -1)
    {
        uint64_t l_FL_Counter = metadataCounter.getNext();
        FL_Write(FLMetaData, TF_Lock, "lock TF, counter=%ld", l_FL_Counter, 0, 0, 0);

        uint64_t l_FL_Counter2 = metadataCounter.getNext();
        FL_Write(FLMetaData, TF_Open, "open TF, counter=%ld", l_FL_Counter2, 0, 0, 0);

        threadLocalTrackSyscallPtr->nowTrack(TrackSyscall::opensyscall, l_TagLockFileName, __LINE__);
        fd = open(l_TagLockFileName.c_str(), O_WRONLY);
        threadLocalTrackSyscallPtr->clearTrack();

        FL_Write(FLMetaData, TF_Open_End, "open TF, counter=%ld, fd=%ld", l_FL_Counter2, fd, 0, 0);

        if (fd >= 0)
        {
            // Exclusive lock and this will block if needed
            LOG(bb,debug) << "lock(): Open issued for tag lockfile " << l_TagLockFileName << ", fd=" << fd;
            struct flock l_LockOptions;
            l_LockOptions.l_whence = SEEK_SET;
            l_LockOptions.l_start = 0;
            l_LockOptions.l_len = 0;    // Lock entire file for writing
            l_LockOptions.l_type = F_WRLCK;
            threadLocalTrackSyscallPtr->nowTrack(TrackSyscall::fcntlsyscall, l_TagLockFileName, __LINE__);
            rc = ::fcntl(fd, F_SETLKW, &l_LockOptions);
            threadLocalTrackSyscallPtr->clearTrack();
        }

        switch(rc)
        {
            case -2:
            {
                errorText << "Could not open tag lockfile " << l_TagLockFileName << " for locking, errno=" << errno << ":" << strerror(errno);
                LOG_ERROR_TEXT_ERRNO(errorText, errno);
            }
            break;

            case -1:
            {
                errorText << "Could not exclusively lock tag lockfile " << l_TagLockFileName << ", errno=" << errno << ":" << strerror(errno);
                LOG_ERROR_TEXT_ERRNO(errorText, errno);

                if (fd >= 0)
                {
                    LOG(bb,debug) << "lock(): Issue close for tagfile fd " << fd;
                    uint64_t l_FL_Counter = metadataCounter.getNext();
                    FL_Write(FLMetaData, TF_CouldNotLockExcl, "open TF, could not lock exclusive, performing close, counter=%ld", l_FL_Counter, 0, 0, 0);
                    ::close(fd);
                    FL_Write(FLMetaData, TF_CouldNotLockExcl_End, "open TF, could not lock exclusive, performing close, counter=%ld, fd=%ld", l_FL_Counter, fd, 0, 0);
                }
            }
            break;

            default:
            {
                // Successful lock...
                tagFileLockFd = fd;
            }
            break;
        }

        FL_Write(FLMetaData, TF_Lock_End, "lock TF, counter=%ld, fd=%ld, rc=%ld, errno=%ld", l_FL_Counter, fd, rc, errno);
    }
    else
    {
        // NOTE:  Should never be the case...
        rc = -1;
        errorText << "Tag lockfile " << l_TagLockFileName << " is currently locked";
        LOG_ERROR_TEXT_RC(errorText, rc);
    }

    return rc;
}


/*
 * Non-static methods
 */
int BBTagHandle::compareContrib(vector<uint32_t>& pContribVector)
{
    int rc = 0;

    size_t l_NumExpectContrib = pContribVector.size();
    if (l_NumExpectContrib == expectContrib.size())
    {
        for (size_t i=0; i<l_NumExpectContrib; ++i)
        {
            if (pContribVector[i] != expectContrib[i])
            {
                rc = 1;
                break;
            }
        }
    }
    else
    {
        rc = 1;
    }

    return rc;
}

int TagFile::save()
{
    int rc = 0;

    uint64_t l_FL_Counter = metadataCounter.getNext();
    FL_Write(FLMetaData, TF_Save, "saveTagFile, counter=%ld", l_FL_Counter, 0, 0, 0);

    try
    {
        LOG(bb,debug) << "Writing:" << filename;
        ofstream l_ArchiveFile{filename};
        text_oarchive l_Archive{l_ArchiveFile};
        l_Archive << *this;
    }
    catch(ExceptionBailout& e) { }
    catch(exception& e)
    {
        rc = -1;
        LOG_ERROR_RC_WITH_EXCEPTION(__FILE__, __FUNCTION__, __LINE__, e, rc);
    }

    FL_Write(FLMetaData, TF_Save_End, "saveTagFile, counter=%ld, rc=%ld", l_FL_Counter, rc, 0, 0);

    return rc;
}

void TagFile::unlock()
{
    if (tagFileLockFd != -1)
    {
        unlock(tagFileLockFd);
        tagFileLockFd = -1;
    }

    return;
}

void TagFile::unlock(const int pFd)
{
    stringstream errorText;

    uint64_t l_FL_Counter = metadataCounter.getNext();
    FL_Write(FLMetaData, TF_Unlock, "unlock TF, counter=%ld, fd=%ld", l_FL_Counter, pFd, 0, 0);

    if (pFd >= 0)
    {
        try
        {
            // Unlock the file
            struct flock l_LockOptions;
            l_LockOptions.l_whence = SEEK_SET;
            l_LockOptions.l_start = 0;
            l_LockOptions.l_len = 0;    // Unlock entire file
            l_LockOptions.l_type = F_UNLCK;
            LOG(bb,debug) << "unlock(): Issue unlock for tagfile fd " << pFd;
            threadLocalTrackSyscallPtr->nowTrack(TrackSyscall::fcntlsyscall, pFd, __LINE__);
            int rc = ::fcntl(pFd, F_SETLK, &l_LockOptions);
            threadLocalTrackSyscallPtr->clearTrack();
            if (!rc)
            {
                // Successful unlock...
            }
            else
            {
                LOG(bb,warning) << "Could not exclusively unlock tagfile fd " << pFd << ", errno=" << errno << ":" << strerror(errno);
            }
            LOG(bb,debug) << "close(): Issue close for tagfile fd " << pFd;

            ::close(pFd);

            FL_Write(FLMetaData, TF_Close_End, "close TF, counter=%ld, fd=%ld", l_FL_Counter, pFd, 0, 0);
        }
        catch(exception& e)
        {
            LOG(bb,info) << "Exception caught " << __func__ << "@" << __FILE__ << ":" << __LINE__ << " what=" << e.what();
        }
    }

    FL_Write(FLMetaData, TF_Unlock_End, "unlock TF, counter=%ld, fd=%ld", l_FL_Counter, pFd, 0, 0);

    return;
}