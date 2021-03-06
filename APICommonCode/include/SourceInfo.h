/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2008 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 */
/*
    File:       SourceInfo.h

    Contains:   This object contains an interface for getting at any bit
                of "interesting" information regarding a content source in a
                format - independent manner.
                
                For instance, the derived object SDPSourceInfo parses an
                SDP file and retrieves all the SourceInfo information from that file.
                    
    

*/

#ifndef __SOURCE_INFO_H__
#define __SOURCE_INFO_H__

#include <CF/StrPtrLen.h>
#include <CF/Core/Time.h>

#include "QTSS.h"

class SourceInfo {
 public:

  SourceInfo() : fStreamArray(nullptr), fNumStreams(0),
                 fOutputArray(nullptr), fNumOutputs(0),
                 fTimeSet(false), fStartTimeUnixSecs(0), fEndTimeUnixSecs(0),
                 fSessionControlType(kRTSPSessionControl), fHasValidTime(false) {}
  SourceInfo(const SourceInfo &copy);// Does copy dynamically allocated data
  virtual ~SourceInfo(); // Deletes the dynamically allocated data

  enum {
    eDefaultBufferDelay = 3
  };

  // Returns whether this source is reflectable.
  bool IsReflectable();

  // Each source is comprised of a set of streams. Those streams have
  // the following metadata.
  struct StreamInfo {
    StreamInfo()
        : fSrcIPAddr(0),
          fDestIPAddr(0),
          fPort(0),
          fTimeToLive(0),
          fPayloadType(0),
          fPayloadName(),
          fTrackID(0),
          fTrackName(),
          fBufferDelay((Float32) eDefaultBufferDelay),
          fIsTCP(false),
          fSetupToReceive(false),
          fTimeScale(0) {}
    ~StreamInfo(); // Deletes the memory allocated for the fPayloadName string

    void Copy(const StreamInfo &copy);// Does copy dynamically allocated data

    /*
     * NOTE: where we can set fSrcIPAddr?
     */

    UInt32 fSrcIPAddr;                // Src IP address of content (this may be 0 if not known for sure)
    UInt32 fDestIPAddr;               // from 'c=' line, Dest IP address of content (destination IP addr for source broadcast!)
    UInt16 fPort;                     // from 'm=' line, Dest (RTP) port of source content
    UInt16 fTimeToLive;               // from 'c=' line, Ttl for this stream
    QTSS_RTPPayloadType fPayloadType; // from 'm=' line, Payload type of this stream
    CF::StrPtrLen fPayloadName;       // from 'a=rtpmap:' line, Payload name of this stream
    UInt32 fTrackID;                  // from 'a=control:' line, ID of this stream
    CF::StrPtrLen fTrackName;         // from 'a=control:' line, Track Name of this stream
    Float32 fBufferDelay;             // from 'a=x-bufferdelay:' line, buffer delay (default is 3 seconds)
    bool fIsTCP;                      // from 'm=' line, Is this a TCP broadcast? If this is the case, the port and ttl are not valid
    bool fSetupToReceive;             // If true then a push to the server is setup on this stream.
    UInt32 fTimeScale;
  };

  // Returns the number of StreamInfo objects (number of Streams in this source)
  UInt32 GetNumStreams() { return fNumStreams; }
  StreamInfo *GetStreamInfo(UInt32 inStreamIndex);
  StreamInfo *GetStreamInfoByTrackID(UInt32 inTrackID);

  // If this source is to be Relayed, it may have "Output" information. This
  // tells the reader where to forward the incoming streams onto. There may be
  // 0 -> N OutputInfo objects in this SourceInfo. Each OutputInfo refers to a
  // single, complete copy of ALL the input streams. The fPortArray field
  // contains one RTP port for each incoming stream.
  struct OutputInfo {
    OutputInfo()
        : fDestAddr(0),
          fLocalAddr(0),
          fTimeToLive(0),
          fPortArray(nullptr),
          fNumPorts(0),
          fBasePort(0),
          fAlreadySetup(false) {}
    ~OutputInfo(); // Deletes the memory allocated for fPortArray

    // Returns true if the two are equal
    bool Equal(const OutputInfo &info);

    void Copy(const OutputInfo &copy);// Does copy dynamically allocated data

    UInt32 fDestAddr;       // Destination address to forward the input onto
    UInt32 fLocalAddr;      // Address of local interface to send out on (may be 0)
    UInt16 fTimeToLive;     // Time to live for resulting output (if multicast)
    UInt16 *fPortArray;     // 1 destination RTP port for each Stream.
    UInt32 fNumPorts;       // Size of the fPortArray (usually equal to fNumStreams)
    UInt16 fBasePort;       // The base destination RTP port - for i=1 to fNumStreams fPortArray[i] = fPortArray[i-1] + 2
    bool fAlreadySetup;     // A flag used in QTSSReflectorModule.cpp
  };

  // Returns the number of OutputInfo objects.
  UInt32 GetNumOutputs() { return fNumOutputs; }
  UInt32 GetNumNewOutputs(); // Returns # of outputs not already setup

  OutputInfo *GetOutputInfo(UInt32 inOutputIndex);

  // GetLocalSDP. This may or may not be supported by sources. Typically, if
  // the source is reflectable, this must be supported. It returns a newly
  // allocated buffer (that the caller is responsible for) containing an SDP
  // description of the source, stripped of all network info.
  virtual char *GetLocalSDP(UInt32 * /*newSDPLen*/) { return nullptr; }

  // This is only supported by the RTSPSourceInfo sub class
  virtual bool IsRTSPSourceInfo() { return false; }

  // This is only supported by the RCFSourceInfo sub class and its derived classes
  virtual char *Name() { return nullptr; }

  virtual bool Equal(SourceInfo *inInfo);

  // SDP scheduled times supports earliest start and latest end -- doesn't handle repeat times or multiple active times.
#define kNTP_Offset_From_1970 2208988800LU
  time_t NTPSecs_to_UnixSecs(time_t time) { return (time_t) (time - (UInt32) kNTP_Offset_From_1970); }
  UInt32 UnixSecs_to_NTPSecs(time_t time) { return (UInt32) (time + (UInt32) kNTP_Offset_From_1970); }
  bool SetActiveNTPTimes(UInt32 startNTPTime, UInt32 endNTPTime);
  bool IsValidNTPSecs(UInt32 time) { return time >= (UInt32) kNTP_Offset_From_1970; }
  bool IsPermanentSource() { return (fStartTimeUnixSecs == 0) && (fEndTimeUnixSecs == 0); }
  bool IsActiveTime(time_t unixTimeSecs);
  bool IsActiveNow() { return IsActiveTime(CF::Core::Time::UnixTime_Secs()); }
  bool IsRTSPControlled() { return fSessionControlType == kRTSPSessionControl; }
  bool HasTCPStreams();
  bool HasIncomingBroacast();
  time_t GetStartTimeUnixSecs() { return fStartTimeUnixSecs; }
  time_t GetEndTimeUnixSecs() { return fEndTimeUnixSecs; }
  UInt32 GetDurationSecs();

  enum { kSDPTimeControl, kRTSPSessionControl };
 protected:

  //utility function used by IsReflectable
  bool IsReflectableIPAddr(UInt32 inIPAddr);

  StreamInfo *fStreamArray;
  UInt32 fNumStreams;        // count 'm=' line

  OutputInfo *fOutputArray;
  UInt32 fNumOutputs;

  bool fTimeSet;             // 't=' line exist?
  time_t fStartTimeUnixSecs; // from 't=' line
  time_t fEndTimeUnixSecs;   // from 't=' line

  UInt32 fSessionControlType; // from 'a=x-broadcastcontrol:' line
  bool fHasValidTime;
};

#endif //__SOURCE_INFO_H__
