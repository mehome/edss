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
    File:       RTSPReflectorOutput.h

    Contains:   Derived from ReflectorOutput, this implements the WritePacket
                method in terms of the QTSS API (that is, it writes to a client
                using the QTSS_RTPSessionObject



*/

#ifndef __RTSP_REFLECTOR_OUTPUT_H__
#define __RTSP_REFLECTOR_OUTPUT_H__

#include "ReflectorOutput.h"
#include "ReflectorSession.h"
#include "QTSS.h"

class RTPSessionOutput : public ReflectorOutput {
 public:

  // Adds some dictionary attributes
  static void Register();

  RTPSessionOutput(QTSS_ClientSessionObject inRTPSession, ReflectorSession *inReflectorSession, QTSS_Object serverPrefs, QTSS_AttributeID inCookieAddrID);
  ~RTPSessionOutput() override = default;

  ReflectorSession *GetReflectorSession() { return fReflectorSession; }
  void InitializeStreams();

  // This writes the packet out to the proper QTSS_RTPStreamObject.
  // If this function returns QTSS_WouldBlock, timeToSendThisPacketAgain will
  // be set to # of msec in which the packet can be sent, or -1 if unknown
  QTSS_Error WritePacket(CF::StrPtrLen *inPacketData, void *inStreamCookie, UInt32 inFlags, SInt64 packetLatenessInMSec,
                         SInt64 *timeToSendThisPacketAgain, UInt64 *packetIDPtr, SInt64 *arrivalTimeMSec, bool firstPacket) override;
  void TearDown() override;

  SInt64 GetReflectorSessionInitTime() { return fReflectorSession->GetInitTimeMS(); }

  bool IsUDP() override;

  bool IsPlaying() override;

  void SetBufferDelay(UInt32 delay) { fBufferDelayMSecs = delay; }

 private:

  QTSS_ClientSessionObject fClientSession;
  ReflectorSession *fReflectorSession;
  QTSS_AttributeID fCookieAttrID; // is sStreamCookieAttr that defined in QTSSReflectorModule
  UInt32 fBufferDelayMSecs;
  SInt64 fBaseArrivalTime;
  bool fIsUDP;
  bool fTransportInitialized;
  bool fMustSynch;
  bool fPreFilter;

  UInt16 GetPacketSeqNumber(CF::StrPtrLen *inPacket);
  void SetPacketSeqNumber(CF::StrPtrLen *inPacket, UInt16 inSeqNumber);
  bool PacketShouldBeThinned(QTSS_RTPStreamObject inStream, CF::StrPtrLen *inPacket);
  bool FilterPacket(QTSS_RTPStreamObject *theStreamPtr, CF::StrPtrLen *inPacket);

  UInt32 GetPacketRTPTime(CF::StrPtrLen *packetStrPtr);
  inline bool PacketMatchesStream(void *inStreamCookie, QTSS_RTPStreamObject *theStreamPtr);
  bool PacketReadyToSend(QTSS_RTPStreamObject *theStreamPtr, SInt64 *currentTimePtr, UInt32 inFlags, UInt64 *packetIDPtr, SInt64 *timeToSendThisPacketAgainPtr);
  bool PacketAlreadySent(QTSS_RTPStreamObject *theStreamPtr, UInt32 inFlags, UInt64 *packetIDPtr);
  QTSS_Error TrackRTCPBaseTime(QTSS_RTPStreamObject *theStreamPtr, CF::StrPtrLen *inPacketStrPtr, SInt64 *currentTimePtr, UInt32 inFlags, SInt64 *packetLatenessInMSec, SInt64 *timeToSendThisPacketAgain, UInt64 *packetIDPtr, SInt64 *arrivalTimeMSecPtr);
  QTSS_Error RewriteRTCP(QTSS_RTPStreamObject *theStreamPtr, CF::StrPtrLen *inPacketStrPtr, SInt64 *currentTimePtr, UInt32 inFlags, SInt64 *packetLatenessInMSec, SInt64 *timeToSendThisPacketAgain, UInt64 *packetIDPtr, SInt64 *arrivalTimeMSecPtr);
  QTSS_Error TrackRTPPackets(QTSS_RTPStreamObject *theStreamPtr, CF::StrPtrLen *inPacketStrPtr, SInt64 *currentTimePtr, UInt32 inFlags, SInt64 *packetLatenessInMSec, SInt64 *timeToSendThisPacketAgain, UInt64 *packetIDPtr, SInt64 *arrivalTimeMSecPtr);
  QTSS_Error TrackRTCPPackets(QTSS_RTPStreamObject *theStreamPtr, CF::StrPtrLen *inPacketStrPtr, SInt64 *currentTimePtr, UInt32 inFlags, SInt64 *packetLatenessInMSec, SInt64 *timeToSendThisPacketAgain, UInt64 *packetIDPtr, SInt64 *arrivalTimeMSecPtr);
  QTSS_Error TrackPackets(QTSS_RTPStreamObject *theStreamPtr, CF::StrPtrLen *inPacketStrPtr, SInt64 *currentTimePtr, UInt32 inFlags, SInt64 *packetLatenessInMSec, SInt64 *timeToSendThisPacketAgain, UInt64 *packetIDPtr, SInt64 *arrivalTimeMSecPtr);
};

bool RTPSessionOutput::PacketMatchesStream(void *inStreamCookie, QTSS_RTPStreamObject *theStreamPtr) {
  void **theStreamCookie = nullptr;
  UInt32 theLen = 0;
  (void) QTSS_GetValuePtr(*theStreamPtr, fCookieAttrID, 0, (void **) &theStreamCookie, &theLen);

  // in fact, the cookie is the pointer of ReflectorStream
  return (theStreamCookie != nullptr) && (*theStreamCookie == inStreamCookie);
}

#endif //__RTSP_REFLECTOR_OUTPUT_H__
