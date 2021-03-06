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
/**
 * @file QTSSReflectorModule.cpp
 *
 * Implementation of QTSSReflectorModule class.
 */

#include <CF/ArrayObjectDeleter.h>
#include <CF/Net/Http/QueryParamList.h>

#include "QTSServerInterface.h"
#include "QTSSReflectorModule.h"
#include "QTSSModuleUtils.h"
#include "ReflectorSession.h"
#include "QTSSMemoryDeleter.h"
#include "QTAccessFile.h"

#include "RTPSessionOutput.h"
#include "SDPSourceInfo.h"

#include "SDPUtils.h"
#include <SDPCache.h>


using namespace std;

#ifndef __Win32__

#endif

#ifndef DEBUG_REFLECTOR_SESSION
#define DEBUG_REFLECTOR_SESSION 0
#else
#undef DEBUG_REFLECTOR_SESSION
#define DEBUG_REFLECTOR_SESSION 1
#endif

#ifndef DEBUG_REFLECTOR_MODULE
#define DEBUG_REFLECTOR_MODULE 0
#else
#undef DEBUG_REFLECTOR_MODULE
#define DEBUG_REFLECTOR_MODULE 1
#endif

using namespace CF;

// ATTRIBUTES
static QTSS_AttributeID sOutputAttr = qtssIllegalAttrID;
//static QTSS_AttributeID sSessionAttr = qtssIllegalAttrID;
static QTSS_AttributeID sStreamCookieAttr = qtssIllegalAttrID;
static QTSS_AttributeID sRequestBodyAttr = qtssIllegalAttrID;
static QTSS_AttributeID sBufferOffsetAttr = qtssIllegalAttrID;
static QTSS_AttributeID sExpectedDigitFilenameErr = qtssIllegalAttrID;
static QTSS_AttributeID sReflectorBadTrackIDErr = qtssIllegalAttrID;
static QTSS_AttributeID sDuplicateBroadcastStreamErr = qtssIllegalAttrID;
static QTSS_AttributeID sClientBroadcastSessionAttr = qtssIllegalAttrID; // ReflectorSession in RTPSession
static QTSS_AttributeID sRTSPBroadcastSessionAttr = qtssIllegalAttrID;   // ReflectorSession in RTSPSession
static QTSS_AttributeID sAnnounceRequiresSDPinNameErr = qtssIllegalAttrID;
static QTSS_AttributeID sAnnounceDisabledNameErr = qtssIllegalAttrID;
static QTSS_AttributeID sSDPcontainsInvalidMinimumPortErr = qtssIllegalAttrID;
static QTSS_AttributeID sSDPcontainsInvalidMaximumPortErr = qtssIllegalAttrID;
static QTSS_AttributeID sStaticPortsConflictErr = qtssIllegalAttrID;
static QTSS_AttributeID sInvalidPortRangeErr = qtssIllegalAttrID;

static QTSS_AttributeID sKillClientsEnabledAttr = qtssIllegalAttrID;
static QTSS_AttributeID sRTPInfoWaitTimeAttr = qtssIllegalAttrID;

// STATIC DATA

// ref to the prefs dictionary object
static RefTable *sSessionMap = nullptr;
static const StrPtrLen kCacheControlHeader("no-cache");
static QTSS_PrefsObject sServerPrefs = nullptr;
static QTSS_ServerObject sServer = nullptr;
static QTSS_ModulePrefsObject sPrefs = nullptr;

//
// Prefs
static bool sAllowNonSDPURLs = true;
static bool sDefaultAllowNonSDPURLs = true;

static bool sRTPInfoDisabled = false;
static bool sDefaultRTPInfoDisabled = false;

static bool sAnnounceEnabled = true;
static bool sDefaultAnnounceEnabled = true;
static bool sBroadcastPushEnabled = true;
static bool sDefaultBroadcastPushEnabled = true;
static bool sAllowDuplicateBroadcasts = false;
static bool sDefaultAllowDuplicateBroadcasts = false;

static UInt32 sMaxBroadcastAnnounceDuration = 0;
static UInt32 sDefaultMaxBroadcastAnnounceDuration = 0;
static UInt16 sMinimumStaticSDPPort = 0;
static UInt16 sDefaultMinimumStaticSDPPort = 20000;
static UInt16 sMaximumStaticSDPPort = 0;
static UInt16 sDefaultMaximumStaticSDPPort = 65535;

static bool sTearDownClientsOnDisconnect = false;
static bool sDefaultTearDownClientsOnDisconnect = false;

static bool sOneSSRCPerStream = true;
static bool sDefaultOneSSRCPerStream = true;

static UInt32 sTimeoutSSRCSecs = 30;
static UInt32 sDefaultTimeoutSSRCSecs = 30;

static UInt32 sBroadcasterSessionTimeoutSecs = 30;
static UInt32 sDefaultBroadcasterSessionTimeoutSecs = 30;
static UInt32 sBroadcasterSessionTimeoutMilliSecs = sBroadcasterSessionTimeoutSecs * 1000;

static UInt16 sLastMax = 0;
static UInt16 sLastMin = 0;

static bool sEnforceStaticSDPPortRange = false;
static bool sDefaultEnforceStaticSDPPortRange = false;

static UInt32 sMaxAnnouncedSDPLengthInKbytes = 4;
//static UInt32   sDefaultMaxAnnouncedSDPLengthInKbytes = 4;

static QTSS_AttributeID sIPAllowListID = qtssIllegalAttrID;
static char *sIPAllowList = nullptr;
static char *sLocalLoopBackAddress = "127.0.0.*";

static bool sAuthenticateLocalBroadcast = false;
static bool sDefaultAuthenticateLocalBroadcast = false;

static bool sDisableOverbuffering = false;
static bool sDefaultDisableOverbuffering = false;
static bool sFalse = false;

static bool sReflectBroadcasts = true;
static bool sDefaultReflectBroadcasts = true;

static bool sAnnouncedKill = true;
static bool sDefaultAnnouncedKill = true;

static bool sPlayResponseRangeHeader = true;
static bool sDefaultPlayResponseRangeHeader = true;

static bool sPlayerCompatibility = true;
static bool sDefaultPlayerCompatibility = true;

static UInt32 sAdjustMediaBandwidthPercent = 100;
static UInt32 sAdjustMediaBandwidthPercentDefault = 100;

static bool sForceRTPInfoSeqAndTime = false;
static bool sDefaultForceRTPInfoSeqAndTime = false;

static char *sRedirectBroadcastsKeyword = nullptr;
static char *sDefaultRedirectBroadcastsKeyword = "";
static char *sBroadcastsRedirectDir = nullptr;
static char *sDefaultBroadcastsRedirectDir = ""; // match none
static char *sDefaultBroadcastsDir = ""; // match all
static char *sDefaultsBroadcasterGroup = "broadcaster";
static StrPtrLen sBroadcasterGroup;

static QTSS_AttributeID sBroadcastDirListID = qtssIllegalAttrID;

static SInt32 sWaitTimeLoopCount = 10;

// Important strings
static StrPtrLen sSDPKillSuffix(".kill");
static StrPtrLen sSDPSuffix(".sdp");
static StrPtrLen sMOVSuffix(".mov");
static StrPtrLen sSDPTooLongMessage("Announced SDP is too long");
static StrPtrLen sSDPNotValidMessage("Announced SDP is not a valid SDP");
static StrPtrLen sKILLNotValidMessage("Announced .kill is not a valid SDP");
static StrPtrLen sSDPTimeNotValidMessage("SDP time is not valid or movie not available at this time.");
static StrPtrLen sBroadcastNotAllowed("Broadcast is not allowed.");
static StrPtrLen sBroadcastNotActive("Broadcast is not active.");
static StrPtrLen sTheNowRangeHeader("npt=now-");

// FUNCTION PROTOTYPES

static QTSS_Error QTSSReflectorModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams);

static QTSS_Error Register(QTSS_Register_Params *inParams);

static QTSS_Error Initialize(QTSS_Initialize_Params *inParams);

static QTSS_Error Shutdown();

static QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params *inParams);

static QTSS_Error DoAnnounce(QTSS_StandardRTSP_Params *inParams);

static QTSS_Error DoDescribe(QTSS_StandardRTSP_Params *inParams);

ReflectorSession *FindOrCreateSession(StrPtrLen *inName, QTSS_StandardRTSP_Params *inParams, UInt32 inChannel=0,
                                      StrPtrLen *inData=nullptr, bool isPush=false, bool *foundSessionPtr=nullptr);

static QTSS_Error DoSetup(QTSS_StandardRTSP_Params *inParams);

static QTSS_Error DoPlay(QTSS_StandardRTSP_Params *inParams, ReflectorSession *inSession);

static QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params *inParams);

static void RemoveOutput(ReflectorOutput *inOutput, ReflectorSession *inSession, bool killClients);

static ReflectorSession *DoSessionSetup(QTSS_StandardRTSP_Params *inParams, QTSS_AttributeID inPathType, bool isPush = false,
                                        bool *foundSessionPtr = nullptr, char **resultFilePath = nullptr);

static QTSS_Error RereadPrefs();

static QTSS_Error ProcessRTPData(QTSS_IncomingData_Params *inParams);

static QTSS_Error ReflectorAuthorizeRTSPRequest(QTSS_StandardRTSP_Params *inParams);

static bool InfoPortsOK(QTSS_StandardRTSP_Params *inParams, SDPSourceInfo *theInfo, StrPtrLen *inPath);

void KillCommandPathInList();

bool KillSession(StrPtrLen *sdpPath, bool killClients);

QTSS_Error IntervalRole();

static bool AcceptSession(QTSS_StandardRTSP_Params *inParams);

static QTSS_Error RedirectBroadcast(QTSS_StandardRTSP_Params *inParams);

static bool AllowBroadcast(QTSS_RTSPRequestObject inRTSPRequest);

static bool InBroadcastDirList(QTSS_RTSPRequestObject inRTSPRequest);

static bool IsAbsolutePath(StrPtrLen *inPathPtr);

static QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params *inParams);

inline void KeepSession(QTSS_RTSPRequestObject theRequest, bool keep) {
  (void) QTSS_SetValue(theRequest, qtssRTSPReqRespKeepAlive, 0, &keep, sizeof(keep));
}

// FUNCTION IMPLEMENTATIONS
QTSS_Error QTSSReflectorModule_Main(void *inPrivateArgs) {
  return _stublibrary_main(inPrivateArgs, QTSSReflectorModuleDispatch);
}

QTSS_Error QTSSReflectorModuleDispatch(QTSS_Role inRole, QTSS_RoleParamPtr inParams) {
  switch (inRole) {
    case QTSS_Register_Role:             return Register(&inParams->regParams);
    case QTSS_Initialize_Role:           return Initialize(&inParams->initParams);
    case QTSS_RereadPrefs_Role:          return RereadPrefs();
    case QTSS_RTSPRoute_Role:            return RedirectBroadcast(&inParams->rtspRouteParams);
    case QTSS_RTSPPreProcessor_Role:     return ProcessRTSPRequest(&inParams->rtspRequestParams);
    case QTSS_RTSPIncomingData_Role:     return ProcessRTPData(&inParams->rtspIncomingDataParams);
    case QTSS_ClientSessionClosing_Role: return DestroySession(&inParams->clientSessionClosingParams);
    case QTSS_Shutdown_Role:             return Shutdown();
    case QTSS_RTSPAuthorize_Role:        return ReflectorAuthorizeRTSPRequest(&inParams->rtspRequestParams);
    case QTSS_Interval_Role:             return IntervalRole();
    case Easy_GetDeviceStream_Role:      return GetDeviceStream(&inParams->easyGetDeviceStreamParams);
    default:break;
  }
  return QTSS_NoErr;
}

QTSS_Error Register(QTSS_Register_Params *inParams) {
  // Do role & attribute setup
  (void) QTSS_AddRole(QTSS_Initialize_Role);
  (void) QTSS_AddRole(QTSS_Shutdown_Role);
  (void) QTSS_AddRole(QTSS_RTSPPreProcessor_Role);
  (void) QTSS_AddRole(QTSS_ClientSessionClosing_Role);
  (void) QTSS_AddRole(QTSS_RTSPIncomingData_Role);
  (void) QTSS_AddRole(QTSS_RTSPAuthorize_Role);
  (void) QTSS_AddRole(QTSS_RereadPrefs_Role);
  (void) QTSS_AddRole(QTSS_RTSPRoute_Role);
  (void) QTSS_AddRole(Easy_GetDeviceStream_Role);

  // Add text messages attributes
  static const char *sExpectedDigitFilenameName = "QTSSReflectorModuleExpectedDigitFilename";
  static const char *sReflectorBadTrackIDErrName = "QTSSReflectorModuleBadTrackID";
  static const char *sDuplicateBroadcastStreamName = "QTSSReflectorModuleDuplicateBroadcastStream";
  static const char *sAnnounceRequiresSDPinName = "QTSSReflectorModuleAnnounceRequiresSDPSuffix";
  static const char *sAnnounceDisabledName = "QTSSReflectorModuleAnnounceDisabled";

  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sDuplicateBroadcastStreamName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sDuplicateBroadcastStreamName, &sDuplicateBroadcastStreamErr);
  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sAnnounceRequiresSDPinName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sAnnounceRequiresSDPinName, &sAnnounceRequiresSDPinNameErr);
  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sAnnounceDisabledName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sAnnounceDisabledName, &sAnnounceDisabledNameErr);

  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sExpectedDigitFilenameName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sExpectedDigitFilenameName, &sExpectedDigitFilenameErr);

  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sReflectorBadTrackIDErrName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sReflectorBadTrackIDErrName, &sReflectorBadTrackIDErr);

  static const char *sSDPcontainsInvalidMinumumPortErrName = "QTSSReflectorModuleSDPPortMinimumPort";
  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sSDPcontainsInvalidMinumumPortErrName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sSDPcontainsInvalidMinumumPortErrName, &sSDPcontainsInvalidMinimumPortErr);

  static const char *sSDPcontainsInvalidMaximumPortErrName = "QTSSReflectorModuleSDPPortMaximumPort";
  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sSDPcontainsInvalidMaximumPortErrName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sSDPcontainsInvalidMaximumPortErrName, &sSDPcontainsInvalidMaximumPortErr);

  static const char *sStaticPortsConflictErrName = "QTSSReflectorModuleStaticPortsConflict";
  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sStaticPortsConflictErrName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sStaticPortsConflictErrName, &sStaticPortsConflictErr);

  static const char *sInvalidPortRangeErrName = "QTSSReflectorModuleStaticPortPrefsBadRange";
  (void) QTSS_AddStaticAttribute(qtssTextMessagesObjectType, sInvalidPortRangeErrName, nullptr, qtssAttrDataTypeCharArray);
  (void) QTSS_IDForAttr(qtssTextMessagesObjectType, sInvalidPortRangeErrName, &sInvalidPortRangeErr);

  // Add an RTP session attribute for tracking ReflectorSession objects
  static const char *sOutputName = "QTSSReflectorModuleOutput";
//  static char *sSessionName = "QTSSReflectorModuleSession";
  static const char *sStreamCookieName = "QTSSReflectorModuleStreamCookie";
  static const char *sRequestBufferName = "QTSSReflectorModuleRequestBuffer";
  static const char *sRequestBufferLenName = "QTSSReflectorModuleRequestBufferLen";
  static const char *sBroadcasterSessionName = "QTSSReflectorModuleBroadcasterSession";
  static const char *sKillClientsEnabledName = "QTSSReflectorModuleTearDownClients";

  static const char *sRTPInfoWaitTime = "QTSSReflectorModuleRTPInfoWaitTime";
  (void) QTSS_AddStaticAttribute(qtssClientSessionObjectType, sRTPInfoWaitTime, nullptr, qtssAttrDataTypeSInt32);
  (void) QTSS_IDForAttr(qtssClientSessionObjectType, sRTPInfoWaitTime, &sRTPInfoWaitTimeAttr);

  (void) QTSS_AddStaticAttribute(qtssClientSessionObjectType, sOutputName, nullptr, qtssAttrDataTypeVoidPointer);
  (void) QTSS_IDForAttr(qtssClientSessionObjectType, sOutputName, &sOutputAttr);

//  (void) QTSS_AddStaticAttribute(qtssClientSessionObjectType, sSessionName, NULL, qtssAttrDataTypeVoidPointer);
//  (void) QTSS_IDForAttr(qtssClientSessionObjectType, sSessionName, &sSessionAttr);

  (void) QTSS_AddStaticAttribute(qtssRTPStreamObjectType, sStreamCookieName, nullptr, qtssAttrDataTypeVoidPointer);
  (void) QTSS_IDForAttr(qtssRTPStreamObjectType, sStreamCookieName, &sStreamCookieAttr);

  (void) QTSS_AddStaticAttribute(qtssRTSPRequestObjectType, sRequestBufferName, nullptr, qtssAttrDataTypeVoidPointer);
  (void) QTSS_IDForAttr(qtssRTSPRequestObjectType, sRequestBufferName, &sRequestBodyAttr);

  (void) QTSS_AddStaticAttribute(qtssRTSPRequestObjectType, sRequestBufferLenName, nullptr, qtssAttrDataTypeUInt32);
  (void) QTSS_IDForAttr(qtssRTSPRequestObjectType, sRequestBufferLenName, &sBufferOffsetAttr);

  (void) QTSS_AddStaticAttribute(qtssClientSessionObjectType, sBroadcasterSessionName, nullptr, qtssAttrDataTypeVoidPointer);
  (void) QTSS_IDForAttr(qtssClientSessionObjectType, sBroadcasterSessionName, &sClientBroadcastSessionAttr);

  (void) QTSS_AddStaticAttribute(qtssClientSessionObjectType, sKillClientsEnabledName, nullptr, qtssAttrDataTypeBool16);
  (void) QTSS_IDForAttr(qtssClientSessionObjectType, sKillClientsEnabledName, &sKillClientsEnabledAttr);

  // keep the same attribute name for the RTSPSessionObject as used int he ClientSessionObject
  (void) QTSS_AddStaticAttribute(qtssRTSPSessionObjectType, sBroadcasterSessionName, nullptr, qtssAttrDataTypeVoidPointer);
  (void) QTSS_IDForAttr(qtssRTSPSessionObjectType, sBroadcasterSessionName, &sRTSPBroadcastSessionAttr);

  // Reflector session needs to setup some parameters too.
  ReflectorStream::Register();
  // RTPSessionOutput needs to do the same
  RTPSessionOutput::Register();

  // Tell the server our name!
  static const char *sModuleName = "QTSSReflectorModule";
  ::strcpy(inParams->outModuleName, sModuleName);

  return QTSS_NoErr;
}

QTSS_Error Initialize(QTSS_Initialize_Params *inParams) {
  // Setup module utils
  QTSSModuleUtils::Initialize(inParams->inMessages, inParams->inServer, inParams->inErrorLogStream);
  QTAccessFile::Initialize();
  sSessionMap = QTSServerInterface::GetServer()->GetReflectorSessionMap();
  sServerPrefs = inParams->inPrefs;
  sServer = inParams->inServer;
#if QTSS_REFLECTOR_EXTERNAL_MODULE
  // The reflector is dependent on a number of objects in the Common Utilities
  // library that get setup by the server if the reflector is internal to the
  // server.
  //
  // So, if the reflector is being built as a code fragment, it must initialize
  // those pieces itself
#if !MACOSXEVENTQUEUE
  ::select_startevents();//initialize the select() implementation of the event queue
#endif
  OS::Initialize();
  Socket::Initialize();
  SocketUtils::Initialize();

  const UInt32 kNumReflectorThreads = 8;
  TaskThreadPool::AddThreads(kNumReflectorThreads);
  IdleTask::Initialize();
  Socket::StartThread();
#endif

  sPrefs = QTSSModuleUtils::GetModulePrefsObject(inParams->inModule);

  // Call helper class initializers
  ReflectorStream::Initialize(sPrefs);
  ReflectorSession::Initialize();

  // Report to the server that this module handles DESCRIBE, SETUP, PLAY, PAUSE, and TEARDOWN
  static QTSS_RTSPMethod sSupportedMethods[] = {
      qtssDescribeMethod, qtssSetupMethod, qtssTeardownMethod, qtssPlayMethod,
      qtssPauseMethod, qtssAnnounceMethod, qtssRecordMethod
  };
  QTSSModuleUtils::SetupSupportedMethods(inParams->inServer, sSupportedMethods, 7);

  RereadPrefs();

  return QTSS_NoErr;
}

char *GetTrimmedKeyWord(char *prefKeyWord) {
  StrPtrLen redirKeyWordStr(prefKeyWord);
  StringParser theRequestPathParser(&redirKeyWordStr);

  // trim leading / from the keyword
  while (theRequestPathParser.Expect(kPathDelimiterChar));

  StrPtrLen theKeyWordStr;
  theRequestPathParser.ConsumeUntil(&theKeyWordStr, kPathDelimiterChar); // stop when we see a / and don't include

  auto *keyword = new char[theKeyWordStr.Len + 1];
  ::memcpy(keyword, theKeyWordStr.Ptr, theKeyWordStr.Len);
  keyword[theKeyWordStr.Len] = 0;

  return keyword;
}

void SetMoviesRelativeDir() {
  char *movieFolderString = nullptr;
  (void) QTSS_GetValueAsString(sServerPrefs, qtssPrefsMovieFolder, 0, &movieFolderString);
  CharArrayDeleter deleter(movieFolderString);

  ResizeableStringFormatter redirectPath(nullptr, 0);
  redirectPath.Put(movieFolderString);
  if (redirectPath.GetBytesWritten() > 0 && kPathDelimiterChar != redirectPath.GetBufPtr()[redirectPath.GetBytesWritten() - 1])
    redirectPath.PutChar(kPathDelimiterChar);
  redirectPath.Put(sBroadcastsRedirectDir);

  auto *newMovieRelativeDir = new char[redirectPath.GetBytesWritten() + 1];
  ::memcpy(newMovieRelativeDir, redirectPath.GetBufPtr(), redirectPath.GetBytesWritten());
  newMovieRelativeDir[redirectPath.GetBytesWritten()] = 0;

  delete[] sBroadcastsRedirectDir;
  sBroadcastsRedirectDir = newMovieRelativeDir;

}

QTSS_Error RereadPrefs() {
  //
  // Use the standard GetPref routine to retrieve the correct values for our preferences
  QTSSModuleUtils::GetAttribute(sPrefs, "disable_rtp_play_info", qtssAttrDataTypeBool16, &sRTPInfoDisabled, &sDefaultRTPInfoDisabled, sizeof(sDefaultRTPInfoDisabled));

  QTSSModuleUtils::GetAttribute(sPrefs, "allow_non_sdp_urls", qtssAttrDataTypeBool16, &sAllowNonSDPURLs, &sDefaultAllowNonSDPURLs, sizeof(sDefaultAllowNonSDPURLs));

  QTSSModuleUtils::GetAttribute(sPrefs, "enable_broadcast_announce", qtssAttrDataTypeBool16, &sAnnounceEnabled, &sDefaultAnnounceEnabled, sizeof(sDefaultAnnounceEnabled));
  QTSSModuleUtils::GetAttribute(sPrefs, "enable_broadcast_push", qtssAttrDataTypeBool16, &sBroadcastPushEnabled, &sDefaultBroadcastPushEnabled, sizeof(sDefaultBroadcastPushEnabled));
  QTSSModuleUtils::GetAttribute(sPrefs, "max_broadcast_announce_duration_secs", qtssAttrDataTypeUInt32, &sMaxBroadcastAnnounceDuration, &sDefaultMaxBroadcastAnnounceDuration, sizeof(sDefaultMaxBroadcastAnnounceDuration));
  QTSSModuleUtils::GetAttribute(sPrefs, "allow_duplicate_broadcasts", qtssAttrDataTypeBool16, &sAllowDuplicateBroadcasts, &sDefaultAllowDuplicateBroadcasts, sizeof(sDefaultAllowDuplicateBroadcasts));

  QTSSModuleUtils::GetAttribute(sPrefs, "enforce_static_sdp_port_range", qtssAttrDataTypeBool16, &sEnforceStaticSDPPortRange, &sDefaultEnforceStaticSDPPortRange, sizeof(sDefaultEnforceStaticSDPPortRange));
  QTSSModuleUtils::GetAttribute(sPrefs, "minimum_static_sdp_port", qtssAttrDataTypeUInt16, &sMinimumStaticSDPPort, &sDefaultMinimumStaticSDPPort, sizeof(sDefaultMinimumStaticSDPPort));
  QTSSModuleUtils::GetAttribute(sPrefs, "maximum_static_sdp_port", qtssAttrDataTypeUInt16, &sMaximumStaticSDPPort, &sDefaultMaximumStaticSDPPort, sizeof(sDefaultMaximumStaticSDPPort));

  QTSSModuleUtils::GetAttribute(sPrefs, "kill_clients_when_broadcast_stops", qtssAttrDataTypeBool16, &sTearDownClientsOnDisconnect, &sDefaultTearDownClientsOnDisconnect, sizeof(sDefaultTearDownClientsOnDisconnect));
  QTSSModuleUtils::GetAttribute(sPrefs, "use_one_SSRC_per_stream", qtssAttrDataTypeBool16, &sOneSSRCPerStream, &sDefaultOneSSRCPerStream, sizeof(sDefaultOneSSRCPerStream));
  QTSSModuleUtils::GetAttribute(sPrefs, "timeout_stream_SSRC_secs", qtssAttrDataTypeUInt32, &sTimeoutSSRCSecs, &sDefaultTimeoutSSRCSecs, sizeof(sDefaultTimeoutSSRCSecs));

  QTSSModuleUtils::GetAttribute(sPrefs, "timeout_broadcaster_session_secs", qtssAttrDataTypeUInt32, &sBroadcasterSessionTimeoutSecs, &sDefaultBroadcasterSessionTimeoutSecs, sizeof(sDefaultTimeoutSSRCSecs));

  if (sBroadcasterSessionTimeoutSecs < 30)
    sBroadcasterSessionTimeoutSecs = 30;

  QTSSModuleUtils::GetAttribute(sPrefs, "authenticate_local_broadcast", qtssAttrDataTypeBool16, &sAuthenticateLocalBroadcast, &sDefaultAuthenticateLocalBroadcast, sizeof(sDefaultAuthenticateLocalBroadcast));

  QTSSModuleUtils::GetAttribute(sPrefs, "disable_overbuffering", qtssAttrDataTypeBool16, &sDisableOverbuffering, &sDefaultDisableOverbuffering, sizeof(sDefaultDisableOverbuffering));

  QTSSModuleUtils::GetAttribute(sPrefs, "allow_broadcasts", qtssAttrDataTypeBool16, &sReflectBroadcasts, &sDefaultReflectBroadcasts, sizeof(sDefaultReflectBroadcasts));

  QTSSModuleUtils::GetAttribute(sPrefs, "allow_announced_kill", qtssAttrDataTypeBool16, &sAnnouncedKill, &sDefaultAnnouncedKill, sizeof(sDefaultAnnouncedKill));

  QTSSModuleUtils::GetAttribute(sPrefs, "enable_play_response_range_header", qtssAttrDataTypeBool16, &sPlayResponseRangeHeader, &sDefaultPlayResponseRangeHeader, sizeof(sDefaultPlayResponseRangeHeader));

  QTSSModuleUtils::GetAttribute(sPrefs, "enable_player_compatibility", qtssAttrDataTypeBool16, &sPlayerCompatibility, &sDefaultPlayerCompatibility, sizeof(sDefaultPlayerCompatibility));

  QTSSModuleUtils::GetAttribute(sPrefs, "compatibility_adjust_sdp_media_bandwidth_percent", qtssAttrDataTypeUInt32, &sAdjustMediaBandwidthPercent, &sAdjustMediaBandwidthPercentDefault, sizeof(sAdjustMediaBandwidthPercentDefault));

  if (sAdjustMediaBandwidthPercent > 100)
    sAdjustMediaBandwidthPercent = 100;

  if (sAdjustMediaBandwidthPercent < 1)
    sAdjustMediaBandwidthPercent = 1;

  QTSSModuleUtils::GetAttribute(sPrefs, "force_rtp_info_sequence_and_time", qtssAttrDataTypeBool16, &sForceRTPInfoSeqAndTime, &sDefaultForceRTPInfoSeqAndTime, sizeof(sDefaultForceRTPInfoSeqAndTime));

  sBroadcasterGroup.Delete();
  sBroadcasterGroup.Set(QTSSModuleUtils::GetStringAttribute(sPrefs, "BroadcasterGroup", sDefaultsBroadcasterGroup));

  delete[] sRedirectBroadcastsKeyword;
  char *tempKeyWord = QTSSModuleUtils::GetStringAttribute(sPrefs, "redirect_broadcast_keyword", sDefaultRedirectBroadcastsKeyword);

  sRedirectBroadcastsKeyword = GetTrimmedKeyWord(tempKeyWord);
  delete[] tempKeyWord;

  delete[] sBroadcastsRedirectDir;
  sBroadcastsRedirectDir = QTSSModuleUtils::GetStringAttribute(sPrefs, "redirect_broadcasts_dir", sDefaultBroadcastsRedirectDir);
  if (sBroadcastsRedirectDir && sBroadcastsRedirectDir[0] != kPathDelimiterChar)
    SetMoviesRelativeDir();

  delete[] QTSSModuleUtils::GetStringAttribute(sPrefs, "broadcast_dir_list", sDefaultBroadcastsDir); // initialize if there isn't one
  sBroadcastDirListID = QTSSModuleUtils::GetAttrID(sPrefs, "broadcast_dir_list");

  delete[] sIPAllowList;
  sIPAllowList = QTSSModuleUtils::GetStringAttribute(sPrefs, "ip_allow_list", sLocalLoopBackAddress);
  sIPAllowListID = QTSSModuleUtils::GetAttrID(sPrefs, "ip_allow_list");

  sBroadcasterSessionTimeoutMilliSecs = sBroadcasterSessionTimeoutSecs * 1000;

  if (sEnforceStaticSDPPortRange) {
    bool reportErrors = false;
    if (sLastMax != sMaximumStaticSDPPort) {
      sLastMax = sMaximumStaticSDPPort;
      reportErrors = true;
    }

    if (sLastMin != sMinimumStaticSDPPort) {
      sLastMin = sMinimumStaticSDPPort;
      reportErrors = true;
    }

    if (reportErrors) {
      UInt16 minServerPort = 6970;
      UInt16 maxServerPort = 9999;
      char min[32];
      char max[32];

      if (((sMinimumStaticSDPPort <= minServerPort) && (sMaximumStaticSDPPort >= minServerPort)) ||
          ((sMinimumStaticSDPPort >= minServerPort) && (sMinimumStaticSDPPort <= maxServerPort))) {
        s_sprintf(min, "%u", minServerPort);
        s_sprintf(max, "%u", maxServerPort);
        QTSSModuleUtils::LogError(qtssWarningVerbosity, sStaticPortsConflictErr, 0, min, max);
      }

      if (sMinimumStaticSDPPort > sMaximumStaticSDPPort) {
        s_sprintf(min, "%u", sMinimumStaticSDPPort);
        s_sprintf(max, "%u", sMaximumStaticSDPPort);
        QTSSModuleUtils::LogError(qtssWarningVerbosity, sInvalidPortRangeErr, 0, min, max);
      }
    }
  }

  KillCommandPathInList();

  return QTSS_NoErr;
}

QTSS_Error Shutdown() {
#if QTSS_REFLECTOR_EXTERNAL_MODULE
  TaskThreadPool::RemoveThreads();
#endif
  return QTSS_NoErr;
}

QTSS_Error IntervalRole() { // not used
  (void) QTSS_SetIntervalRoleTimer(0); // turn off

  return QTSS_NoErr;
}

/**
 * process RTP data from RTSP Interleaved Frame
 */
QTSS_Error ProcessRTPData(QTSS_IncomingData_Params *inParams) {

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:ProcessRTPData inRTSPSession=%" _U32BITARG_ " inClientSession=%" _U32BITARG_ "\n",
            inParams->inRTSPSession, inParams->inClientSession);

  if (!sBroadcastPushEnabled)
    return QTSS_NoErr;

  ReflectorSession *theSession = nullptr;
  UInt32 theLen = sizeof(theSession);
  QTSS_Error theErr = QTSS_GetValue(inParams->inRTSPSession, sRTSPBroadcastSessionAttr, 0, &theSession, &theLen); // set in DoPlay()

  if (theErr == QTSS_ValueNotFound) {
    return QTSS_NoErr;
  }

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule.cpp:ProcessRTPData    sClientBroadcastSessionAttr=%" _U32BITARG_ " theSession=%" _U32BITARG_ " err=%" _S32BITARG_ " \n",
            sClientBroadcastSessionAttr, theSession,theErr);

  if (theSession == nullptr || theErr != QTSS_NoErr) return QTSS_NoErr;

  // it is a broadcaster session
  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule.cpp:is broadcaster session\n");

  SourceInfo *theSoureInfo = theSession->GetSourceInfo();
  Assert(theSoureInfo != nullptr);
  if (theSoureInfo == nullptr) return QTSS_NoErr;

  UInt32 numStreams = theSession->GetNumStreams();

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule.cpp:ProcessRTPData numStreams=%" _U32BITARG_ "\n",
            numStreams);

  {
    /*
     * Stream data such as RTP packets is encapsulated by an ASCII dollar
     * sign (0x24), followed by a one-byte channel identifier,
     * followed by the length of the encapsulated binary data as a binary,
     * two-byte integer in network byte order. The stream data follows
     * immediately afterwards, without a CRLF, but including the upper-layer
     * protocol headers. Each $ block contains exactly one upper-layer
     * protocol data unit, e.g., one RTP packet.
     *
     * @see RTSPSession::HandleIncomingDataPacket
     *
     */
    char *packetData = inParams->inPacketData;

    UInt8 packetChannel;
    packetChannel = (UInt8) packetData[1];

    UInt16 rtpPacketLen;
    memcpy(&rtpPacketLen, &packetData[2], 2);
    rtpPacketLen = ntohs(rtpPacketLen);

    char *rtpPacket = &packetData[4]; // 剥离 Interleaved Header

    DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
              "QTSSReflectorModule.cpp:ProcessRTPData channel=%u theSoureInfo=%" _U32BITARG_ " packetLen=%" _U32BITARG_ " packetDatalen=%u\n",
              (UInt16) packetChannel, theSoureInfo, inParams->inPacketLen, rtpPacketLen);

    UInt32 inIndex = packetChannel >> 1U; // one stream per every 2 channels rtcp channel handled below
    if (inIndex < numStreams) {
      ReflectorStream *theStream = theSession->GetStreamByIndex(inIndex);
      if (theStream == nullptr) return QTSS_Unimplemented;
      auto isRTCP = static_cast<bool>(packetChannel & 1U);

      if (1) {
        SourceInfo::StreamInfo *theStreamInfo = theStream->GetStreamInfo();
        UInt16 serverReceivePort = theStreamInfo->fPort;

        if (isRTCP) serverReceivePort++;

        // TODO(james): localServerAddr
        DEBUG_LOG(0,
                  "QTSSReflectorModule.cpp:ProcessRTPData Send RTSP packet channel=%u to UDP localServerAddr=%" _U32BITARG_ " serverReceivePort=%" _U32BITARG_ " packetDataLen=%u \n",
                  (UInt16) packetChannel, theStream->GetSocketPair()->GetSocketA()->GetLocalAddr(), serverReceivePort, rtpPacketLen);
      }

      // 将 packet 转交给 ReflectorStream，随后跟 ReflectorSocket 的处理逻辑归并为一
      theStream->PushPacket(rtpPacket, rtpPacketLen, isRTCP);
    }
  }

  return theErr;
}

QTSS_Error ProcessRTSPRequest(QTSS_StandardRTSP_Params *inParams) {
  Core::MutexLocker locker(sSessionMap->GetMutex()); //operating on sOutputAttr

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:ProcessRTSPRequest inClientSession=%p\n",
            inParams->inClientSession);

  QTSS_RTSPMethod *theMethod = nullptr;
  UInt32 theLen = 0;
  if ((QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqMethod, 0, (void **) &theMethod, &theLen) != QTSS_NoErr) ||
      (theLen != sizeof(QTSS_RTSPMethod))) {
    Assert(0);
    return QTSS_RequestFailed;
  }

  if (*theMethod == qtssAnnounceMethod) return DoAnnounce(inParams);
  if (*theMethod == qtssDescribeMethod) return DoDescribe(inParams);
  if (*theMethod == qtssSetupMethod) return DoSetup(inParams);

  RTPSessionOutput **theOutput = nullptr;
  QTSS_Error theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void **) &theOutput, &theLen);
  if ((theErr != QTSS_NoErr) || (theLen != sizeof(RTPSessionOutput *))) { // a broadcaster push session
    if (*theMethod == qtssPlayMethod || *theMethod == qtssRecordMethod)
      return DoPlay(inParams, nullptr);
    else // 不能响应推流端的 teardown 请求
      return QTSS_RequestFailed;
  }

  switch (*theMethod) {
    case qtssPlayMethod:
      return DoPlay(inParams, (*theOutput)->GetReflectorSession());
    case qtssTeardownMethod:
      // Tell the server that this session should be killed, and send a TEARDOWN response
      (void) QTSS_Teardown(inParams->inClientSession);
      (void) QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, 0);
      break;
    case qtssPauseMethod:
      (void) QTSS_Pause(inParams->inClientSession);
      (void) QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, 0);
      break;
    default:break;
  }
  return QTSS_NoErr;
}

/**
 * 调用FindOrCreateSession来对哈希表sSessionMap进行查询
 *
 * @param inPathType the attribute id of path. for setup method, it is qtssRTSPReqFilePathTrunc; otherwise is qtssRTSPReqFilePath
 */
ReflectorSession *DoSessionSetup(QTSS_StandardRTSP_Params *inParams, QTSS_AttributeID inPathType, bool isPush,
                                 bool *foundSessionPtr, char **resultFilePath) {

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:DoSessionSetup inClientSession=%p isPash=%d\n",
            inParams->inClientSession, isPush);

  char* theFullPathStr = nullptr;
  QTSS_Error theErr = QTSS_GetValueAsString(inParams->inRTSPRequest, inPathType, 0, &theFullPathStr);
  Assert(theErr == QTSS_NoErr);
  QTSSCharArrayDeleter theFullPathStrDeleter(theFullPathStr);

  if (theErr != QTSS_NoErr) return nullptr;

//  char *theQueryString = NULL;
//  theErr = QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqQueryString, 0, &theQueryString);
//  QTSSCharArrayDeleter theQueryStringDeleter(theQueryString);

//  std::string queryTemp;
//  if (theQueryString) {
//    queryTemp = EasyUtil::Urldecode(theQueryString);
//  }
//  Net::QueryParamList parList(const_cast<char *>(queryTemp.c_str()));

  UInt32 theChannelNum = 1;
//  char const *chnNum = parList.DoFindCGIValueForParam(EASY_TAG_CHANNEL);
//  if (chnNum) {
//    theChannelNum = (UInt32) stoi(chnNum);
//  }

  StrPtrLen theFullPath(theFullPathStr);

  if (theFullPath.Len > sMOVSuffix.Len) {
    StrPtrLen endOfPath2(&theFullPath.Ptr[theFullPath.Len - sMOVSuffix.Len], sMOVSuffix.Len);
    if (endOfPath2.Equal(sMOVSuffix)) {  // it is a .mov so it is not meant for us
      return nullptr;
    }
  }

  // 推模式不允许 non-sdp url
  if (sAllowNonSDPURLs && !isPush) {
    // Check and see if the full path to this file matches an existing ReflectorSession
    StrPtrLen thePathPtr;
    CharArrayDeleter sdpPath(QTSSModuleUtils::GetFullPath(inParams->inRTSPRequest, inPathType, &thePathPtr.Len, &sSDPSuffix));

    thePathPtr.Ptr = sdpPath.GetObject();

    // If the actual file path has a .sdp in it, first look for the URL without the extra .sdp
    if (thePathPtr.Len > (sSDPSuffix.Len * 2)) {
      // Check and see if there is a .sdp in the file path.
      // If there is, truncate off our extra ".sdp", cuz it isn't needed
      StrPtrLen endOfPath(&thePathPtr.Ptr[thePathPtr.Len - (sSDPSuffix.Len * 2)], sSDPSuffix.Len);
      if (endOfPath.Equal(sSDPSuffix)) {
        thePathPtr.Ptr[thePathPtr.Len - sSDPSuffix.Len] = '\0';
        thePathPtr.Len -= sSDPSuffix.Len;
      }
    }
    if (resultFilePath != nullptr) *resultFilePath = thePathPtr.GetAsCString();
    return FindOrCreateSession(&thePathPtr, inParams, theChannelNum);
  } else {
    if (isPush && !sDefaultBroadcastPushEnabled)
      return nullptr;

    //
    // We aren't supposed to auto-append a .sdp, so just get the URL path out of the server
    if (theFullPath.Len > sSDPSuffix.Len) {
      //
      // Check to make sure this path has a .sdp at the end. If it does,
      // attempt to get a reflector session for this URL.
      StrPtrLen endOfPath2(&theFullPath.Ptr[theFullPath.Len - sSDPSuffix.Len], sSDPSuffix.Len);
      if (endOfPath2.Equal(sSDPSuffix)) {
        if (resultFilePath != nullptr) *resultFilePath = theFullPath.GetAsCString();
        return FindOrCreateSession(&theFullPath, inParams, theChannelNum, nullptr, isPush, foundSessionPtr);
      }
    }
    return nullptr;
  }
}

void DoAnnounceAddRequiredSDPLines(QTSS_StandardRTSP_Params *inParams, ResizeableStringFormatter *editedSDP, char *theSDPPtr) {
  SDPContainer checkedSDPContainer;
  checkedSDPContainer.SetSDPBuffer(theSDPPtr);
  if (!checkedSDPContainer.HasReqLines()) {
    if (!checkedSDPContainer.HasLineType('v')) { // add v line
      editedSDP->Put("v=0\r\n");
    }

    if (!checkedSDPContainer.HasLineType('s')) { // add s line
      char *theSDPName = nullptr;

      (void) QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFilePath, 0, &theSDPName);
      QTSSCharArrayDeleter thePathStrDeleter(theSDPName);
      if (theSDPName == nullptr) {
        editedSDP->Put("s=unknown\r\n");
      } else {
        editedSDP->Put("s=");
        editedSDP->Put(theSDPName);
        editedSDP->PutEOL();
      }
    }

    if (!checkedSDPContainer.HasLineType('t')) { // add t line
      editedSDP->Put("t=0 0\r\n");
    }

    if (!checkedSDPContainer.HasLineType('o')) { // add o line
      editedSDP->Put("o=");
      char tempBuff[256] = "";
      tempBuff[255] = 0;
      char *nameStr = tempBuff;
      UInt32 buffLen = sizeof(tempBuff) - 1;
      (void) QTSS_GetValue(inParams->inClientSession, qtssCliSesFirstUserAgent, 0, nameStr, &buffLen);
      for (UInt32 c = 0; c < buffLen; c++) {
        if (StringParser::sEOLWhitespaceMask[(UInt8) nameStr[c]]) {
          nameStr[c] = 0;
          break;
        }
      }

      buffLen = ::strlen(nameStr);
      if (buffLen == 0)
        editedSDP->Put("announced_broadcast");
      else
        editedSDP->Put(nameStr, buffLen);

      editedSDP->Put(" ");

      buffLen = sizeof(tempBuff) - 1;
      (void) QTSS_GetValue(inParams->inClientSession, qtssCliSesRTSPSessionID, 0, &tempBuff, &buffLen);
      editedSDP->Put(tempBuff, buffLen);

      editedSDP->Put(" ");
      s_snprintf(tempBuff, sizeof(tempBuff) - 1, "%" _64BITARG_ "d", (SInt64) Core::Time::UnixTime_Secs() + 2208988800LU);
      editedSDP->Put(tempBuff);

      editedSDP->Put(" IN IP4 ");
      buffLen = sizeof(tempBuff) - 1;
      (void) QTSS_GetValue(inParams->inClientSession, qtssCliRTSPSessRemoteAddrStr, 0, tempBuff, &buffLen);
      editedSDP->Put(tempBuff, buffLen);

      editedSDP->PutEOL();
    }
  }

  editedSDP->Put(theSDPPtr);
}

QTSS_Error DoAnnounce(QTSS_StandardRTSP_Params *inParams) {

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:DoAnnounce inClientSession=%p\n",
            inParams->inClientSession);

  // 判断sAnnounceEnabled是否开启，由 enable_broadcast_announce 配置项确定，默认为true
  if (!sAnnounceEnabled)
    return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssPreconditionFailed, sAnnounceDisabledNameErr);

  // If this is SDP data, the reflector has the ability to write the data
  // to the file system location specified by the URL.

  //
  // This is a completely stateless action. No ReflectorSession gets created (obviously).

  //
  // Eventually, we should really require access control before we do this.

  // Get the full path to this file
  char *theFullPathStr = nullptr;
  (void) QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFilePath, 0, &theFullPathStr);
  QTSSCharArrayDeleter theFullPathStrDeleter(theFullPathStr);
  StrPtrLen theFullPath(theFullPathStr);

//  char *theQueryString = NULL;
//  QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqQueryString, 0, &theQueryString);
//  QTSSCharArrayDeleter theQueryStringDeleter(theQueryString);

//  std::string queryTemp;
//  if (theQueryString) {
//    queryTemp = EasyUtil::Urldecode(theQueryString);
//  }
//  QueryParamList parList(const_cast<char *>(queryTemp.c_str()));

  UInt32 theChannelNum = 1;
//  char const *chnNum = parList.DoFindCGIValueForParam(EASY_TAG_CHANNEL);
//  if (chnNum) {
//    theChannelNum = stoi(chnNum);
//  }

  char theStreamName[QTSS_MAX_NAME_LENGTH] = {0};
  sprintf(theStreamName, "%s%s%d", theFullPathStr, EASY_KEY_SPLITER, theChannelNum);

  // Check for a .kill at the end, and set killBroadcast
  bool pathOK = false;
  bool killBroadcast = false;
  if (sAnnouncedKill && theFullPath.Len > sSDPKillSuffix.Len) {
    StrPtrLen endOfPath(theFullPath.Ptr + (theFullPath.Len - sSDPKillSuffix.Len), sSDPKillSuffix.Len);
    if (endOfPath.Equal(sSDPKillSuffix)) {
      pathOK = true;
      killBroadcast = true;
    }
  }

  // Check for a .sdp at the end
  if (!pathOK) {
    if (theFullPath.Len > sSDPSuffix.Len) {
      StrPtrLen endOfPath(theFullPath.Ptr + (theFullPath.Len - sSDPSuffix.Len), sSDPSuffix.Len);
      if (!endOfPath.Equal(sSDPSuffix))
        return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssPreconditionFailed, sAnnounceRequiresSDPinNameErr);
    }
  }

  // TODO: check path exist

  // Ok, this is an sdp file. Retrieve the entire contents of the SDP.
  // This has to be done asynchronously (in case the SDP stuff is fragmented across multiple packets).
  // So, we have to have a simple state machine.

  //
  // We need to know the content length to manage memory
  UInt32 theLen = 0;
  UInt32 *theContentLenP = nullptr;
  QTSS_Error theErr = QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqContentLen, 0, (void **) &theContentLenP, &theLen);
  if ((theErr != QTSS_NoErr) || (theLen != sizeof(UInt32))) {
    //
    // RETURN ERROR RESPONSE: ANNOUNCE WITHOUT CONTENT LENGTH
    return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
  }

  // Check if the content-length is more than the imposed maximum(default is 4*1024).
  // if it is then return error response
  if ((sMaxAnnouncedSDPLengthInKbytes != 0) && (*theContentLenP > (sMaxAnnouncedSDPLengthInKbytes * 1024)))
    return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssPreconditionFailed, &sSDPTooLongMessage);

  //
  // Check for the existence of 2 attributes in the request: a pointer to our buffer for
  // the request body(sRequestBodyAttr), and the current offset in that buffer(sBufferOffsetAttr).
  // If these attributes exist, then we've already been here for this request. If they don't exist, add them.
  UInt32 theBufferOffset = 0;
  char *theRequestBody = nullptr;

  theLen = sizeof(theRequestBody);
  theErr = QTSS_GetValue(inParams->inRTSPRequest, sRequestBodyAttr, 0, &theRequestBody, &theLen);

  //s_printf("QTSSReflectorModule:DoAnnounce theRequestBody =%s\n",theRequestBody);
  if (theErr != QTSS_NoErr) {
    //
    // First time we've been here for this request. Create a buffer for the content body and
    // shove it in the request.
    theRequestBody = new char[*theContentLenP + 1];
    memset(theRequestBody, 0, *theContentLenP + 1);
    theLen = sizeof(theRequestBody);
    theErr = QTSS_SetValue(inParams->inRTSPRequest, sRequestBodyAttr, 0, &theRequestBody, theLen);// SetValue creates an internal copy.
    Assert(theErr == QTSS_NoErr);

    //
    // Also store the offset in the buffer
    theLen = sizeof(theBufferOffset);
    theErr = QTSS_SetValue(inParams->inRTSPRequest, sBufferOffsetAttr, 0, &theBufferOffset, theLen);
    Assert(theErr == QTSS_NoErr);
  } else {
    theLen = sizeof(theBufferOffset);
    theErr = QTSS_GetValue(inParams->inRTSPRequest, sBufferOffsetAttr, 0, &theBufferOffset, &theLen);
  }

  // 通过QTSS_Read从RTSP请求消息inParams->inRTSPRequest中解析出SDP信息

  //
  // We have our buffer and offset. Read the data.
  theErr = QTSS_Read(inParams->inRTSPRequest, theRequestBody + theBufferOffset, *theContentLenP - theBufferOffset, &theLen);
  Assert(theErr != QTSS_BadArgument);

  if (theErr == QTSS_RequestFailed) {
    CharArrayDeleter charArrayPathDeleter(theRequestBody);
    //
    // NEED TO RETURN RTSP ERROR RESPONSE
    return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
  }

  if ((theErr == QTSS_WouldBlock) || (theLen < (*theContentLenP - theBufferOffset))) {
    //
    // Update our offset in the buffer
    theBufferOffset += theLen;
    (void) QTSS_SetValue(inParams->inRTSPRequest, sBufferOffsetAttr, 0, &theBufferOffset, sizeof(theBufferOffset));
    //s_printf("QTSSReflectorModule:DoAnnounce Request some more data \n");
    //
    // The entire content body hasn't arrived yet. Request a read event and wait for it.
    // Our DoAnnounce function will get called again when there is more data.
    theErr = QTSS_RequestEvent(inParams->inRTSPRequest, QTSS_ReadableEvent);
    Assert(theErr == QTSS_NoErr);
    return QTSS_NoErr;
  }

  Assert(theErr == QTSS_NoErr);

  //
  // If we've gotten here, we have the entire content body in our buffer.
  //

  if (killBroadcast) {
    theFullPath.Len -= sSDPKillSuffix.Len;
    if (KillSession(&theFullPath, killBroadcast))
      return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssServerInternal, 0);
    else
      return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientNotFound, &sKILLNotValidMessage);
  }

  // 通过IsSDPBufferValid()方法来校验SDP是否合法，通过InfoPortsOK()判断Announce请求的端口是否合法，
  // 其中端口范围由sMinimumStaticSDPPort、sMaximumStaticSDPPort决定，默认分别为20000、65535

  // ------------  Clean up missing required SDP lines

  ResizeableStringFormatter editedSDP(nullptr, 0);
  DoAnnounceAddRequiredSDPLines(inParams, &editedSDP, theRequestBody);
  StrPtrLen editedSDPSPL(editedSDP.GetBufPtr(), editedSDP.GetBytesWritten());

  // ------------ Check the headers

  SDPContainer checkedSDPContainer;
  checkedSDPContainer.SetSDPBuffer(&editedSDPSPL);
  if (!checkedSDPContainer.IsSDPBufferValid()) {
    return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
  }

  SDPSourceInfo theSDPSourceInfo(editedSDPSPL.Ptr, editedSDPSPL.Len);
  CharArrayDeleter charArrayPathDeleter(theRequestBody);

  if (!InfoPortsOK(inParams, &theSDPSourceInfo, &theFullPath)) { // All validity checks like this check should be done before touching the file.
    return QTSS_NoErr; // InfoPortsOK is sending back the error.
  }

  // ------------ reorder the sdp headers to make them proper.

  SDPLineSorter sortedSDP(&checkedSDPContainer);

  // 分别将SDP字符串中的会话相关字段、媒体相关字段写入sdp文件中.

  // ------------ Write the SDP

  char *sessionHeaders = sortedSDP.GetSessionHeaders()->GetAsCString();
  CharArrayDeleter sessionHeadersDeleter(sessionHeaders);

  char *mediaHeaders = sortedSDP.GetMediaHeaders()->GetAsCString();
  CharArrayDeleter mediaHeadersDeleter(mediaHeaders);

  // sortedSDP.GetSessionHeaders()->PrintStrEOL();
  // sortedSDP.GetMediaHeaders()->PrintStrEOL();

#if 0
  // write the file !! need error reporting
  FILE* theSDPFile = ::fopen(theFullPath.Ptr, "wb");//open
  if (theSDPFile != NULL) {
      s_fprintf(theSDPFile, "%s", sessionHeaders);
      s_fprintf(theSDPFile, "%s", mediaHeaders);
      ::fflush(theSDPFile);
      ::fclose(theSDPFile);
  } else {
      return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientForbidden, 0);
  }
#endif
  char sdpContext[1024] = {0};
  sprintf(sdpContext, "%s%s", sessionHeaders, mediaHeaders);
  SDPCache::GetInstance()->setSdpMap(theStreamName, sdpContext);

  //s_printf("QTSSReflectorModule:DoAnnounce SendResponse OK=200\n");

  return QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, 0);
}

void DoDescribeAddRequiredSDPLines(QTSS_StandardRTSP_Params *inParams, ReflectorSession *theSession,
                                   QTSS_TimeVal modDate, ResizeableStringFormatter *editedSDP, StrPtrLen *theSDPPtr) {
  SDPContainer checkedSDPContainer;
  checkedSDPContainer.SetSDPBuffer(theSDPPtr);
  if (!checkedSDPContainer.HasReqLines()) {
    if (!checkedSDPContainer.HasLineType('v')) { // add v line
      editedSDP->Put("v=0\r\n");
    }

    if (!checkedSDPContainer.HasLineType('s')) { // add s line
      char *theSDPName = nullptr;
      (void) QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFilePath, 0, &theSDPName);
      QTSSCharArrayDeleter thePathStrDeleter(theSDPName);
      editedSDP->Put("s=");
      editedSDP->Put(theSDPName);
      editedSDP->PutEOL();
    }

    if (!checkedSDPContainer.HasLineType('t')) { // add t line
      editedSDP->Put("t=0 0\r\n");
    }

    if (!checkedSDPContainer.HasLineType('o')) { // add o line
      editedSDP->Put("o=broadcast_sdp ");
      char tempBuff[256] = "";
      tempBuff[255] = 0;
      s_snprintf(tempBuff, sizeof(tempBuff) - 1, "%"   _U32BITARG_   "", *(UInt32 *) &theSession);
      editedSDP->Put(tempBuff);

      editedSDP->Put(" ");
      // modified date is in milliseconds.  Convert to NTP seconds as recommended by rfc 2327
      s_snprintf(tempBuff, sizeof(tempBuff) - 1, "%" _64BITARG_ "d", (SInt64) (modDate / 1000) + 2208988800LU);
      editedSDP->Put(tempBuff);

      editedSDP->Put(" IN IP4 ");
      UInt32 buffLen = sizeof(tempBuff) - 1;
      (void) QTSS_GetValue(inParams->inClientSession, qtssCliSesHostName, 0, &tempBuff, &buffLen);
      editedSDP->Put(tempBuff, buffLen);

      editedSDP->PutEOL();
    }
  }

  editedSDP->Put(*theSDPPtr);
}

QTSS_Error DoDescribe(QTSS_StandardRTSP_Params *inParams) {

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:DoDescribe inClientSession=%p\n",
            inParams->inClientSession);

  // 1. 根据路径获取或者创建ReflectorSession，并获取对应请求的sdp文件绝对路径；
  UInt32 theRefCount = 0;
  char *theFilePath = nullptr;
  ReflectorSession *theSession = DoSessionSetup(inParams, qtssRTSPReqFilePath, false, nullptr, &theFilePath);
  CharArrayDeleter tempFilePath(theFilePath);

  if (theSession == nullptr) return QTSS_RequestFailed;

  theRefCount++;

  //	//redis,streamid/serial/channel.sdp,for example "./Movies/\streamid\serial\channel0.sdp"
  //	if(true)
  //	{
  //		//1.get the path
  //		char* theFileNameStr = NULL;
  //		QTSS_Error theErrEx = QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqLocalPath, 0, &theFileNameStr);
  //		Assert(theErrEx == QTSS_NoErr);
  //		QTSSCharArrayDeleter theFileNameStrDeleter(theFileNameStr);
  //
  //		//2.get SessionID
  //		char chStreamId[64]={0};
  //#ifdef __Win32__//it's different between linux and windows
  //
  //		char movieFolder[256] = { 0 };
  //		UInt32 thePathLen = 256;
  //		QTSServerInterface::GetServer()->GetPrefs()->GetMovieFolder(&movieFolder[0], &thePathLen);
  //		StringParser parser(&StrPtrLen(theFileNameStr));
  //		StrPtrLen strName;
  //		parser.ConsumeLength(NULL,thePathLen);
  //		parser.Expect('\\');
  //		parser.ConsumeUntil(&strName,'\\');
  //		memcpy(chStreamId,strName.Ptr,strName.Len);
  //#else
  //
  //#endif
  //		//3.auth the streamid in redis
  //		char chResult = 0;
  //		QTSS_RoleParams theParams;
  //		theParams.JudgeStreamIDParams.inStreanID = chStreamId;
  //		theParams.JudgeStreamIDParams.outresult = &chResult;
  //
  //		UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kRedisJudgeStreamIDRole);
  //		for ( UInt32 currentModule=0;currentModule < numModules; currentModule++)
  //		{
  //			QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kRedisJudgeStreamIDRole, currentModule);
  //			(void)theModule->CallDispatch(Easy_RedisJudgeStreamID_Role, &theParams);
  //		}
  //		//if(chResult == 0)
  //		if(false)
  //		{
  //			sSessionMap->Release(theSession->GetRef());//don't forget
  //			return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest,0);;
  //		}
  //		//auth sucessfully
  //	}
  //	//redis

  // 2. 如果已经有一个输出会话附属到这个客户端会话，那么就删除之；
  RTPSessionOutput **theOutput = nullptr;
  UInt32 theLen = 0;
  QTSS_Error theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void **) &theOutput, &theLen);

  // If there already was an RTPSessionOutput attached to this Client Session, destroy it.
  if (theErr == QTSS_NoErr && theOutput != nullptr) {
    RemoveOutput(*theOutput, (*theOutput)->GetReflectorSession(), false);
    RTPSessionOutput *theOutput2 = nullptr;
    (void) QTSS_SetValue(inParams->inClientSession, sOutputAttr, 0, &theOutput2, sizeof(theOutput2));
  }
  // send the DESCRIBE response

  // above function has signalled that this request belongs to us, so let's respond
  iovec theDescribeVec[3] = {{0}};

  Assert(theSession->GetLocalSDP()->Ptr != nullptr);

  // 3. 读取请求对应的sdp文件，将文件内容解析到StrPtrLen theFileData中；
  StrPtrLen theFileData;
  QTSS_TimeVal outModDate = 0;
  QTSS_TimeVal inModDate = -1;
  (void) QTSSModuleUtils::ReadEntireFile(theFilePath, &theFileData, inModDate, &outModDate);
  CharArrayDeleter fileDataDeleter(theFileData.Ptr);

  // 4. 将连接信息清空，包括ip地址、端口号，如下面示例，同时增加一个字段a=control:*
  // -------------- process SDP to remove connection info and add track IDs, port info, and default c= line

  StrPtrLen theSDPData;
  SDPSourceInfo tempSDPSourceInfo(theFileData.Ptr, theFileData.Len); // will make a copy and delete in destructor
  theSDPData.Ptr = tempSDPSourceInfo.GetLocalSDP(&theSDPData.Len); // returns a new buffer with processed sdp
  CharArrayDeleter sdpDeleter(theSDPData.Ptr); // delete the temp sdp source info buffer returned by GetLocalSDP

  if (theSDPData.Len <= 0) { // can't find it on disk or it failed to parse just use the one in the session.
    // NOTE: 虽然 DoSessionSetup 返回的 theFileName 不是 streamName, ReadEntireFile 获取不到内容, 但这里可以补全
    theSDPData.Ptr = theSession->GetLocalSDP()->Ptr; // this sdp isn't ours it must not be deleted
    theSDPData.Len = theSession->GetLocalSDP()->Len;
  }

  // 5. 检测sdp是否包含v、s、t、o这些字段，如果没有就构造补充进去;
  // ------------  Clean up missing required SDP lines

  ResizeableStringFormatter editedSDP(nullptr, 0);
  DoDescribeAddRequiredSDPLines(inParams, theSession, outModDate, &editedSDP, &theSDPData);
  StrPtrLen editedSDPSPL(editedSDP.GetBufPtr(), editedSDP.GetBytesWritten());

  // 6. SetSDPBuffer会调用SDP的解析方法paser()，在该方法内对SDP解析的同时，分析出该SDP是否合法，赋予属性fValid；
  // ------------ Check the headers

  SDPContainer checkedSDPContainer;
  checkedSDPContainer.SetSDPBuffer(&editedSDPSPL);
  if (!checkedSDPContainer.IsSDPBufferValid()) {
    if (theRefCount) sSessionMap->Release(theSession->GetRef());

    return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssUnsupportedMediaType, &sSDPNotValidMessage);
  }

  // ------------ Put SDP header lines in correct order
  Float32 adjustMediaBandwidthPercent = 1.0;
  bool adjustMediaBandwidth = false;

  if (sPlayerCompatibility)
    adjustMediaBandwidth = QTSSModuleUtils::HavePlayerProfile(sServerPrefs, inParams, QTSSModuleUtils::kAdjustBandwidth);

  if (adjustMediaBandwidth)
    adjustMediaBandwidthPercent = (Float32) (sAdjustMediaBandwidthPercent / 100.0);

  ResizeableStringFormatter buffer;
  SDPContainer *insertMediaLines = nullptr;
  SDPLineSorter sortedSDP(&checkedSDPContainer, adjustMediaBandwidthPercent, insertMediaLines);
  delete insertMediaLines;

  // 7. 将sdp的会话信息、媒体信息附在RTSP消息中响应给客户端.
  // ------------ Write the SDP

  UInt32 sessLen = sortedSDP.GetSessionHeaders()->Len;
  UInt32 mediaLen = sortedSDP.GetMediaHeaders()->Len;
  theDescribeVec[1].iov_base = sortedSDP.GetSessionHeaders()->Ptr;
  theDescribeVec[1].iov_len = sortedSDP.GetSessionHeaders()->Len;

  theDescribeVec[2].iov_base = sortedSDP.GetMediaHeaders()->Ptr;
  theDescribeVec[2].iov_len = sortedSDP.GetMediaHeaders()->Len;

  (void) QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssCacheControlHeader, kCacheControlHeader.Ptr, kCacheControlHeader.Len);
  QTSSModuleUtils::SendDescribeResponse(inParams->inRTSPRequest, inParams->inClientSession, &theDescribeVec[0], 3, sessLen + mediaLen);

  if (theRefCount) sSessionMap->Release(theSession->GetRef());

  DEBUG_LOG(DEBUG_REFLECTOR_SESSION,
            "QTSSReflectorModule.cpp:DoDescribe Session@%p refcount=%" _U32BITARG_ "\n",
            theSession->GetRef(), theSession->GetRef()->GetRefCount());

  return QTSS_NoErr;
}

bool InfoPortsOK(QTSS_StandardRTSP_Params *inParams, SDPSourceInfo *theInfo, StrPtrLen *inPath) {
  // Check the ports based on the Pref whether to enforce a static SDP port range.
  bool isOK = true;

  if (sEnforceStaticSDPPortRange) {
    for (UInt32 x = 0; x < theInfo->GetNumStreams(); x++) {
      UInt16 theInfoPort = theInfo->GetStreamInfo(x)->fPort;
      QTSS_AttributeID theErrorMessageID = qtssIllegalAttrID;
      if (theInfoPort != 0) {
        if (theInfoPort < sMinimumStaticSDPPort)
          theErrorMessageID = sSDPcontainsInvalidMinimumPortErr;
        else if (theInfoPort > sMaximumStaticSDPPort)
          theErrorMessageID = sSDPcontainsInvalidMaximumPortErr;
      }

      if (theErrorMessageID != qtssIllegalAttrID) {
        char thePort[32];
        s_sprintf(thePort, "%u", theInfoPort);

        char *thePath = inPath->GetAsCString();
        CharArrayDeleter charArrayPathDeleter(thePath);

        auto *thePathPort = new char[inPath->Len + 32];
        CharArrayDeleter charArrayPathPortDeleter(thePathPort);

        s_sprintf(thePathPort, "%s:%s", thePath, thePort);
        (void) QTSSModuleUtils::LogError(qtssWarningVerbosity, theErrorMessageID, 0, thePathPort);

        StrPtrLen thePortStr(thePort);
        (void) QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssUnsupportedMediaType, theErrorMessageID, &thePortStr);

        return false;
      }
    }
  }

  return isOK;
}

ReflectorSession *FindOrCreateSession(StrPtrLen *inName, QTSS_StandardRTSP_Params *inParams, UInt32 inChannel,
                                      StrPtrLen *inData, bool isPush, bool *foundSessionPtr) {

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:FindOrCreateSession inClientSession=%p isPash=%d\n",
            inParams->inClientSession, isPush);

  Core::MutexLocker locker(sSessionMap->GetMutex());
  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:FindOrCreateSession lock sSessionMap success\n");

  // Check if broadcast is allowed before doing anything else
  // At this point we know it is a definitely a reflector session
  // It is either incoming automatic broadcast setup or a client setup to view broadcast
  // In either case, verify whether the broadcast is allowed, and send forbidden response back
  if (!AllowBroadcast(inParams->inRTSPRequest)) {
    (void) QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientForbidden, &sBroadcastNotAllowed);
    return nullptr;
  }

  char theStreamName[QTSS_MAX_NAME_LENGTH] = {0};
  sprintf(theStreamName, "%s%s%d", inName->Ptr, EASY_KEY_SPLITER, inChannel);

  StrPtrLen inPath(theStreamName);

  Ref *theSessionRef = sSessionMap->Resolve(&inPath);
  ReflectorSession *theSession = nullptr;

  if (theSessionRef == nullptr) {
    // a) 没有根据inPath路径在哈希表sSessionMap中找到对应的ReflectorSession，如果是推送就new一个.

    if (!isPush) return nullptr;

    StrPtrLen theFileData;
    StrPtrLen theFileDeleteData;

    if (inData == nullptr) {
      (void) QTSSModuleUtils::ReadEntireFile(inPath.Ptr, &theFileDeleteData);
      theFileData = theFileDeleteData;
    } else {
      theFileData = *inData;
    }
    CharArrayDeleter fileDataDeleter(theFileDeleteData.Ptr);

    if (theFileData.Len <= 0) return nullptr;

    auto *theInfo = new SDPSourceInfo(theFileData.Ptr, theFileData.Len); // will make a copy

    // 检查全部的 stream 是否都可以被转发
    if (!theInfo->IsReflectable()) {
      delete theInfo;
#ifdef DEBUG_REFLECTOR_SESSION
      s_printf( "QTSSReflectorModule.cpp:FindOrCreateSession Session =%p source info is not reflectable\n", theSessionRef);
#endif
      return nullptr;
    }

    if (!InfoPortsOK(inParams, theInfo, &inPath)) {
      delete theInfo;
      return nullptr;
    }

    //
    // Setup a ReflectorSession and bind the sockets. If we are negotiating,
    // make sure to let the session know that this is a Push Session so
    // ports may be modified.
    UInt32 theSetupFlag = ReflectorSession::kMarkSetup | ReflectorSession::kIsPushSession;

    theSession = new ReflectorSession(inName, inChannel);
    if (theSession == nullptr) return nullptr;

    theSession->SetHasBufferedStreams(true); // buffer the incoming streams for clients

    // SetupReflectorSession stores theInfo in theSession so DON'T delete the Info if we fail here, leave it alone.
    // deleting the session will delete the info.
    QTSS_Error theErr = theSession->SetupReflectorSession(theInfo, inParams, theSetupFlag, sOneSSRCPerStream, sTimeoutSSRCSecs);
    if (theErr != QTSS_NoErr) {
      // delete theSession;
      SDPCache::GetInstance()->eraseSdpMap(theSession->GetSourceID()->Ptr);
      theSession->DelRedisLive();
      theSession->Signal(Thread::Task::kKillEvent);
      return nullptr;
    }

    //s_printf("Created reflector session = %"   _U32BITARG_   " theInfo=%"   _U32BITARG_   " \n", (UInt32) theSession,(UInt32)theInfo);
    //put the session's ID into the session map.
    theErr = sSessionMap->Register(theSession->GetRef());
    Assert(theErr == QTSS_NoErr);

    // unless we do this, the refcount won't increment (and we'll delete the session prematurely
    //if (!isPush)
    {
      Ref *debug = sSessionMap->Resolve(&inPath);
      Assert(debug == theSession->GetRef());
    }
  } else {
    // b) 如果找到了就直接获取 theSession = (ReflectorSession*)theSessionRef->GetObject();

    DEBUG_LOG(DEBUG_REFLECTOR_SESSION,
              "QTSSReflectorModule.cpp:FindOrCreateSession Session =%p refcount=%" _U32BITARG_ "\n",
              theSessionRef, theSessionRef->GetRefCount());

    // Check if broadcast is allowed before doing anything else
    // At this point we know it is a definitely a reflector session
    // It is either incoming automatic broadcast setup or a client setup to view broadcast
    // In either case, verify whether the broadcast is allowed, and send forbidden response back
    do {
      if (!AllowBroadcast(inParams->inRTSPRequest)) {
        (void) QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientForbidden, &sBroadcastNotAllowed);
        break;
      }

      if (foundSessionPtr != nullptr) *foundSessionPtr = true;

      StrPtrLen theFileData;

      if (inData == nullptr) (void) QTSSModuleUtils::ReadEntireFile(inPath.Ptr, &theFileData);
      CharArrayDeleter charArrayDeleter(theFileData.Ptr);

      if (theFileData.Len <= 0) break;

      auto *theInfo = new SDPSourceInfo(theFileData.Ptr, theFileData.Len);
      if (theInfo == nullptr) break;

      if (!InfoPortsOK(inParams, theInfo, &inPath)) {
        delete theInfo;
        break;
      }

      delete theInfo;

      theSession = (ReflectorSession *) theSessionRef->GetObject();
      if (isPush && theSession && !(theSession->IsSetup())) {
        // NOTE: 因为上一个分支已经设置过 ReflectorSession，此处有些多余
        UInt32 theSetupFlag = ReflectorSession::kMarkSetup | ReflectorSession::kIsPushSession;
        QTSS_Error theErr = theSession->SetupReflectorSession(nullptr, inParams, theSetupFlag);
        if (theErr != QTSS_NoErr) {
          theSession = nullptr;
          break;
        }
      }
    } while (0);

    if (theSession == nullptr)
      sSessionMap->Release(theSessionRef);
  }

  Assert(theSession != nullptr);

  // Turn off overbuffering if the "disable_overbuffering" pref says so
  if (sDisableOverbuffering)
    (void) QTSS_SetValue(inParams->inClientSession, qtssCliSesOverBufferEnabled, 0, &sFalse, sizeof(sFalse));

  return theSession;
}

// ONLY call when performing a setup.
void DeleteReflectorPushSession(QTSS_StandardRTSP_Params *inParams, ReflectorSession *theSession, bool foundSession) {
  if (theSession)
    sSessionMap->Release(theSession->GetRef());

  ReflectorSession *stopSessionProcessing = nullptr;
  QTSS_Error theErr = QTSS_SetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &stopSessionProcessing, sizeof(stopSessionProcessing));
  Assert(theErr == QTSS_NoErr);

  if (foundSession)
    return; // we didn't allocate the session so don't delete

  Ref *theSessionRef = theSession->GetRef();
  if (theSessionRef != nullptr) {
    theSession->TearDownAllOutputs(); // just to be sure because we are about to delete the session.
    sSessionMap->UnRegister(theSessionRef);// we had an error while setting up-- don't let anyone get the session
    //delete theSession;
    SDPCache::GetInstance()->eraseSdpMap(theSession->GetSourceID()->Ptr);
    theSession->DelRedisLive();
    theSession->Signal(Thread::Task::kKillEvent);
  }
}

QTSS_Error AddRTPStream(ReflectorSession *theSession, QTSS_StandardRTSP_Params *inParams,
                        QTSS_RTPStreamObject *newStreamPtr, QTSS_AddStreamFlags inFlags=qtssASFlagsForceUDPTransport) {
  // Ok, this is completely crazy but I can't think of a better way to do this that's
  // safe so we'll do it this way for now. Because the ReflectorStreams use this session's
  // stream queue, we need to make sure that each ReflectorStream is not reflecting to this
  // session while we call QTSS_AddRTPStream. One brutal way to do this is to grab each
  // ReflectorStream's mutex, which will stop every reflector stream from running.
  Assert(newStreamPtr != nullptr);

  if (theSession != nullptr)
    for (UInt32 x = 0; x < theSession->GetNumStreams(); x++)
      theSession->GetStreamByIndex(x)->GetMutex()->Lock();

  //
  // Turn off reliable UDP transport, because we are not yet equipped to do overbuffering.
  QTSS_Error theErr = QTSS_AddRTPStream(inParams->inClientSession, inParams->inRTSPRequest, newStreamPtr, inFlags);

  if (theSession != nullptr)
    for (UInt32 y = 0; y < theSession->GetNumStreams(); y++)
      theSession->GetStreamByIndex(y)->GetMutex()->Unlock();

  return theErr;
}

QTSS_Error DoSetup(QTSS_StandardRTSP_Params *inParams) {

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:DoSetup inClientSession=%p\n",
            inParams->inClientSession);

  // 1. 根据关键字qtssRTSPReqTransportMode判断是否为推模式，具体isPush值由Setup请求中的mode值有关，
  //   mode="receive" || mode="record"表示isPush为true。对应的解析函数为: RTSPRequest::ParseModeSubHeader
  UInt32 theLen = 0;
  UInt32 *transportModePtr = nullptr;
  QTSS_Error theErr = QTSS_GetValuePtr(inParams->inRTSPRequest, qtssRTSPReqTransportMode, 0, (void **) &transportModePtr, &theLen);
  const bool isPush = transportModePtr != nullptr && *transportModePtr == qtssRTPTransportModeRecord;
  bool foundSession = false;

  // 2. Find or Create ReflectorSession
  ReflectorSession *theSession = nullptr;
  if (isPush) {
    // This is an incoming data session.

    UInt32 theLenTemp = sizeof(theSession);
    theErr = QTSS_GetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &theSession, &theLenTemp);
    if (theErr != QTSS_NoErr || theLenTemp != sizeof(ReflectorSession *)) {
      // 调用DoSessionSetup创建或引用已存在的ReflectorSession转发会话
      theSession = DoSessionSetup(inParams, qtssRTSPReqFilePathTrunc, isPush, &foundSession);
      if (theSession == nullptr) return QTSS_RequestFailed;

      // Set the ReflectorSession in the ClientSession
      theErr = QTSS_SetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &theSession, sizeof(theSession));
      Assert(theErr == QTSS_NoErr);

      // Set the ClientSession in ReflectorSession and its ReflectorStream
      theSession->AddBroadcasterClientSession(inParams);

      DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
                "QTSSReflectorModule.cpp:SET Session sClientBroadcastSessionAttr=%" _U32BITARG_ " theSession=%p err=%" _S32BITARG_ " \n",
                (UInt32)sClientBroadcastSessionAttr, theSession, theErr);
      (void) QTSS_SetValue(inParams->inClientSession, qtssCliSesTimeoutMsec, 0, &sBroadcasterSessionTimeoutMilliSecs, sizeof(sBroadcasterSessionTimeoutMilliSecs));
    }
  } else {
    // This is an outcoming data session...

    RTPSessionOutput **theOutput = nullptr;
    theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void **) &theOutput, &theLen);
    if (theErr != QTSS_NoErr || theLen != sizeof(RTPSessionOutput *)) {
      // Do the standard ReflectorSession setup
      theSession = DoSessionSetup(inParams, qtssRTSPReqFilePathTrunc);
      if (theSession == nullptr) return QTSS_RequestFailed;

      // create an RTPSessionOutput, and append to theSession
      auto *theNewOutput = new RTPSessionOutput(inParams->inClientSession, theSession, sServerPrefs, sStreamCookieAttr);
      theSession->AddOutput(theNewOutput, true);
      // 将新建的RTPSessionOutput存储起来，key = sOutputAttr;
      (void) QTSS_SetValue(inParams->inClientSession, sOutputAttr, 0, &theNewOutput, sizeof(theNewOutput));
    } else {
      // Just reuse
      theSession = (*theOutput)->GetReflectorSession();
      if (theSession == nullptr) return QTSS_RequestFailed;
    }
  }

  // 3. 解析track ID，后面会根据这个track id来获取流信息
  // unless there is a digit at the end of this path (representing trackID), don't
  // even bother with the request
  char *theDigitStr = nullptr;
  (void) QTSS_GetValueAsString(inParams->inRTSPRequest, qtssRTSPReqFileDigit, 0, &theDigitStr);
  QTSSCharArrayDeleter theDigitStrDeleter(theDigitStr);
  if (theDigitStr == nullptr) {
    if (isPush) DeleteReflectorPushSession(inParams, theSession, foundSession);
    return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, sExpectedDigitFilenameErr);
  }
  auto theTrackID = static_cast<UInt32>(::strtol(theDigitStr, nullptr, 10));

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule.cpp: Get stream info. trackID=%d\n", theTrackID);

  // Get info about this trackID
  SourceInfo::StreamInfo *theStreamInfo = theSession->GetSourceInfo()->GetStreamInfoByTrackID(theTrackID);
  // If theStreamInfo is NULL, we don't have a legit track, so return an error
  if (theStreamInfo == nullptr) {
    if (isPush) DeleteReflectorPushSession(inParams, theSession, foundSession);
    return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, sReflectorBadTrackIDErr);
  }

  // 4. Setup RTPStream
  if (isPush) {
    DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
              "QTSSReflectorModule.cpp:DoSetup is push setup\n");

    // check duplicate broadcasts
    if (!sAllowDuplicateBroadcasts && theStreamInfo->fSetupToReceive) {
      DeleteReflectorPushSession(inParams, theSession, foundSession);
      return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssPreconditionFailed, sDuplicateBroadcastStreamErr);
    }

    UInt16 theReceiveBroadcastStreamPort = theStreamInfo->fPort;
    theErr = QTSS_SetValue(inParams->inRTSPRequest, qtssRTSPReqSetUpServerPort, 0, &theReceiveBroadcastStreamPort, sizeof(theReceiveBroadcastStreamPort));
    Assert(theErr == QTSS_NoErr);

    QTSS_RTPStreamObject newStream = nullptr;
    theErr = AddRTPStream(theSession, inParams, &newStream); // TODO(james): 为什么需要创建 RTPStream?
    Assert(theErr == QTSS_NoErr);
    if (theErr != QTSS_NoErr) {
      DeleteReflectorPushSession(inParams, theSession, foundSession);
      return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
    }

    // send the setup response
    (void) QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssCacheControlHeader, kCacheControlHeader.Ptr, kCacheControlHeader.Len);
    (void) QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, newStream, 0);

    // 标识流转发的建立
    theStreamInfo->fSetupToReceive = true;

    DEBUG_LOG(DEBUG_REFLECTOR_SESSION,
              "QTSSReflectorModule.cpp:DoSetup [PUSH] Session =%p refcount=%" _U32BITARG_ "\n",
              theSession->GetRef(), theSession->GetRef()->GetRefCount());

    return QTSS_NoErr;
  } else { // !isPush

    StrPtrLen *thePayloadName = &theStreamInfo->fPayloadName;
    QTSS_RTPPayloadType thePayloadType = theStreamInfo->fPayloadType;
    UInt32 theTimeScale = theStreamInfo->fTimeScale;
    if (theTimeScale == 0) theTimeScale = 90000;

    QTSS_RTPStreamObject newStream = nullptr;
    theErr = AddRTPStream(theSession, inParams, &newStream, 0);
    if (theErr != QTSS_NoErr)
      return theErr;

    // Set up dictionary items for this stream
    theErr = QTSS_SetValue(newStream, qtssRTPStrPayloadName, 0, thePayloadName->Ptr, thePayloadName->Len);
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrPayloadType, 0, &thePayloadType, sizeof(thePayloadType));
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrTrackID, 0, &theTrackID, sizeof(theTrackID));
    Assert(theErr == QTSS_NoErr);
    theErr = QTSS_SetValue(newStream, qtssRTPStrTimescale, 0, &theTimeScale, sizeof(theTimeScale));
    Assert(theErr == QTSS_NoErr);

    // We only want to allow over buffering to dynamic rate clients
    SInt32 canDynamicRate = -1;
    theLen = sizeof(canDynamicRate);
    (void) QTSS_GetValue(inParams->inRTSPRequest, qtssRTSPReqDynamicRateState, 0, (void *) &canDynamicRate, &theLen);
    if (canDynamicRate < 1) // -1 no rate field, 0 off
      (void) QTSS_SetValue(inParams->inClientSession, qtssCliSesOverBufferEnabled, 0, &sFalse, sizeof(sFalse));

    // Place the stream cookie in this stream for future reference
    void *theStreamCookie = theSession->GetStreamCookie(theTrackID); // the cookie is the pointer of ReflectorStream
    Assert(theStreamCookie != nullptr);
    theErr = QTSS_SetValue(newStream, sStreamCookieAttr, 0, &theStreamCookie, sizeof(theStreamCookie));
    Assert(theErr == QTSS_NoErr);

    // Set the number of quality levels.
    static const UInt32 sNumQualityLevels = ReflectorSession::kNumQualityLevels;
    theErr = QTSS_SetValue(newStream, qtssRTPStrNumQualityLevels, 0, &sNumQualityLevels, sizeof(sNumQualityLevels));
    Assert(theErr == QTSS_NoErr);

    // send the setup response
    (void) QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssCacheControlHeader, kCacheControlHeader.Ptr, kCacheControlHeader.Len);
    (void) QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, newStream, qtssSetupRespDontWriteSSRC); // 转发不改变 RTP 流中的 SSRC，所以要屏蔽 RTPStream 中的 SSRC 的输出

    DEBUG_LOG(DEBUG_REFLECTOR_SESSION,
              "QTSSReflectorModule.cpp:DoSetup [PULL] Session =%p refcount=%" _U32BITARG_ "\n",
             theSession->GetRef(), theSession->GetRef()->GetRefCount());

    return QTSS_NoErr;
  }
}

/**
 * Check all streams have buffered rtp packet
 */
bool HaveStreamBuffers(QTSS_StandardRTSP_Params *inParams, ReflectorSession *inSession) {
  if (inSession == NULL || inParams == NULL) return false;

  bool haveBufferedStreams = true; // set to false and return if we can't set the packets

  // lock all streams
  for (UInt32 x = 0; x < inSession->GetNumStreams(); x++)
    inSession->GetStreamByIndex(x)->GetMutex()->Lock();

  QTSS_RTPStreamObject *theRef;
  UInt32 theLen;
  for (UInt32 theStreamIndex = 0;
      QTSS_GetValuePtr(inParams->inClientSession, qtssCliSesStreamObjects, theStreamIndex, (void **) &theRef, &theLen) == QTSS_NoErr;
      theStreamIndex++) {

    ReflectorStream *theReflectorStream = inSession->GetStreamByIndex(theStreamIndex);

//    if (!theReflectorStream->HasFirstRTCP())
//      printf("theStreamIndex =%"   _U32BITARG_   " no rtcp\n", theStreamIndex);

//    if (!theReflectorStream->HasFirstRTP())
//      printf("theStreamIndex = %"   _U32BITARG_   " no rtp\n", theStreamIndex);

    if ((theReflectorStream == nullptr) || !theReflectorStream->HasFirstRTP()) {
      haveBufferedStreams = false;
      break;
    }

    UInt16 firstSeqNum = 0;
    UInt32 firstTimeStamp = 0;
    SInt64 packetArrivalTime = 0;

    ReflectorSender *theSender = theReflectorStream->GetRTPSender();
    haveBufferedStreams = theSender->GetFirstPacketInfo(&firstSeqNum, &firstTimeStamp, &packetArrivalTime);
    DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
              "theStreamIndex= %" _U32BITARG_ " haveBufferedStreams=%d, seqnum=%d, timestamp=%" _U32BITARG_ "\n",
              theStreamIndex, haveBufferedStreams, firstSeqNum, firstTimeStamp);

    if (!haveBufferedStreams) break;

    QTSS_Error theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstSeqNumber, 0, &firstSeqNum, sizeof(firstSeqNum));
    Assert(theErr == QTSS_NoErr);

    theErr = QTSS_SetValue(*theRef, qtssRTPStrFirstTimestamp, 0, &firstTimeStamp, sizeof(firstTimeStamp));
    Assert(theErr == QTSS_NoErr);
  }

  // unlock all streams
  for (UInt32 y = 0; y < inSession->GetNumStreams(); y++)
    inSession->GetStreamByIndex(y)->GetMutex()->Unlock();

  return haveBufferedStreams;
}

/* 实际上是调用RTPSession::Play函数, 在该函数里会执行 this->Signal(Task::kStartEvent), 从而导致RTPSession::Run函数运行。
 * 在RTPSession::Run函数里,调用fModule->CallDispatch(QTSS_RTPSendPackets_Role, &theParams)。
 * 因为fModule在 RTSPSession::Run 函数里被 SetPacketSendingModule 函数设置成为 QTSSReflectorModule, 而该Module并不支持
 * QTSS_RTPSendPackets_Role, 所以RTPSession::Run返回 0,从而RTPSession::Run函数不会被TaskThread再次调度。 */
QTSS_Error DoPlay(QTSS_StandardRTSP_Params *inParams, ReflectorSession *inSession) {

  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule:DoPlay inClientSession=%p\n",
            inParams->inClientSession);

  QTSS_Error theErr = QTSS_NoErr;
  UInt32 flags = 0;
  UInt32 theLen = 0;
  bool rtpInfoEnabled = false;

  if (inSession == nullptr) {  // 推送端
    if (!sDefaultBroadcastPushEnabled) return QTSS_RequestFailed;

    theLen = sizeof(inSession);
    theErr = QTSS_GetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &inSession, &theLen);
    if (theErr != QTSS_NoErr) return QTSS_RequestFailed;

    theErr = QTSS_SetValue(inParams->inClientSession, sKillClientsEnabledAttr, 0, &sTearDownClientsOnDisconnect, sizeof(sTearDownClientsOnDisconnect));
    if (theErr != QTSS_NoErr) return QTSS_RequestFailed;

    Assert(inSession != nullptr);

    theErr = QTSS_SetValue(inParams->inRTSPSession, sRTSPBroadcastSessionAttr, 0, &inSession, sizeof(inSession));
    if (theErr != QTSS_NoErr) return QTSS_RequestFailed;

    DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
              "QTSSReflectorModule:SET for sRTSPBroadcastSessionAttr err=%" _S32BITARG_ " id=%" _S32BITARG_ "\n",
              theErr, inParams->inRTSPSession);

    // this code needs to be cleaned up
    // Check and see if the full path to this file matches an existing ReflectorSession
    StrPtrLen thePathPtr;
    CharArrayDeleter sdpPath(QTSSModuleUtils::GetFullPath(inParams->inRTSPRequest, qtssRTSPReqFilePath, &thePathPtr.Len, &sSDPSuffix));

    thePathPtr.Ptr = sdpPath.GetObject();

    // remove trackID designation from the path if it is there
    char *trackStr = thePathPtr.FindString("/trackID=");
    if (trackStr != nullptr && *trackStr != 0) {
      *trackStr = 0; // terminate the string.
      thePathPtr.Len = ::strlen(thePathPtr.Ptr);
    }

    // If the actual file path has a .sdp in it, first look for the URL without the extra .sdp
    if (thePathPtr.Len > (sSDPSuffix.Len * 2)) {
      // Check and see if there is a .sdp in the file path.
      // If there is, truncate off our extra ".sdp", cuz it isn't needed
      StrPtrLen endOfPath(&sdpPath.GetObject()[thePathPtr.Len - (sSDPSuffix.Len * 2)], sSDPSuffix.Len);
      if (endOfPath.Equal(sSDPSuffix)) {
        sdpPath.GetObject()[thePathPtr.Len - sSDPSuffix.Len] = '\0';
        thePathPtr.Len -= sSDPSuffix.Len;
      }
    }

    // do all above so we can add the session to the map with Resolve here.
    // we must only do this once.
    //OSRef* debug = sSessionMap->Resolve(&thePathPtr);
    //if (debug != inSession->GetRef()) {
    //	 return QTSSModuleUtils::SendErrorResponse(inParams->inRTSPRequest, qtssClientBadRequest, 0);
    //}

    KeepSession(inParams->inRTSPRequest, true);
    DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
              "QTSSReflectorModule.cpp:DoPlay (PUSH) inRTSPSession=%p inClientSession=%p\n",
              inParams->inRTSPSession, inParams->inClientSession);
  } else {  // 客户端

    RTPSessionOutput **theOutput = nullptr;
    theLen = 0;
    theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void **) &theOutput, &theLen);
    if ((theErr != QTSS_NoErr) || (theLen != sizeof(RTPSessionOutput *)) || (theOutput == nullptr))
      return QTSS_RequestFailed;

    (*theOutput)->InitializeStreams();

    // Tell the session what the bitrate of this reflection is. This is nice for logging,
    // it also allows the server to scale the TCP buffer size appropriately if we are
    // interleaving the data over TCP. This must be set before calling QTSS_Play so the
    // server can use it from within QTSS_Play
    UInt32 bitsPerSecond = inSession->GetBitRate();
    (void) QTSS_SetValue(inParams->inClientSession, qtssCliSesMovieAverageBitRate, 0, &bitsPerSecond, sizeof(bitsPerSecond));

    if (sPlayResponseRangeHeader) {
      StrPtrLen temp;
      theErr = QTSS_GetValuePtr(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, (void **) &temp.Ptr, &temp.Len);
      if (theErr != QTSS_NoErr)
        QTSS_AppendRTSPHeader(inParams->inRTSPRequest, qtssRangeHeader, sTheNowRangeHeader.Ptr, sTheNowRangeHeader.Len);
    }

    if (sPlayerCompatibility)
      rtpInfoEnabled = QTSSModuleUtils::HavePlayerProfile(sServerPrefs, inParams, QTSSModuleUtils::kRequiresRTPInfoSeqAndTime);

    if (sForceRTPInfoSeqAndTime) rtpInfoEnabled = true;

    if (sRTPInfoDisabled) rtpInfoEnabled = false;

    if (rtpInfoEnabled) {
      flags = qtssPlayRespWriteTrackInfo; //write first timestamp and seq num to rtpinfo

      bool haveBufferedStreams = HaveStreamBuffers(inParams, inSession);
      if (haveBufferedStreams) { // send the cached rtp time and seq number in the response.
        theErr = QTSS_Play(inParams->inClientSession, inParams->inRTSPRequest, qtssPlayRespWriteTrackInfo);
        if (theErr != QTSS_NoErr) return theErr;
      } else {
        SInt32 waitTimeLoopCount = 0;
        theLen = sizeof(waitTimeLoopCount);
        theErr = QTSS_GetValue(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, &waitTimeLoopCount, &theLen);
        if (theErr != QTSS_NoErr) {
          (void) QTSS_SetValue(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, &sWaitTimeLoopCount, sizeof(sWaitTimeLoopCount));
        } else {
          if (waitTimeLoopCount < 1)
            return QTSSModuleUtils::SendErrorResponseWithMessage(inParams->inRTSPRequest, qtssClientNotFound, &sBroadcastNotActive);

          waitTimeLoopCount--;
          (void) QTSS_SetValue(inParams->inClientSession, sRTPInfoWaitTimeAttr, 0, &waitTimeLoopCount, sizeof(waitTimeLoopCount));
        }

        DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
                  "QTSSReflectorModule:DoPlay  wait 100ms waitTimeLoopCount=%ld\n",
                  waitTimeLoopCount);

        SInt64 interval = 1 * 100; // 100 millisecond
        QTSS_SetIdleTimer(interval);
        return QTSS_NoErr;
      }
    } else {
      theErr = QTSS_Play(inParams->inClientSession, inParams->inRTSPRequest, qtssPlayFlagsAppendServerInfo);
      if (theErr != QTSS_NoErr) return theErr;
    }
  }

  // send record/play response
  (void) QTSS_SendStandardRTSPResponse(inParams->inRTSPRequest, inParams->inClientSession, flags);

  DEBUG_LOG(DEBUG_REFLECTOR_SESSION,
            "QTSSReflectorModule.cpp:DoPlay Session =%p refcount=%" _U32BITARG_ "\n",
            inSession->GetRef(), inSession->GetRef()->GetRefCount());

  return QTSS_NoErr;
}

bool KillSession(StrPtrLen *sdpPathStr, bool killClients) {
  Ref *theSessionRef = sSessionMap->Resolve(sdpPathStr);
  if (theSessionRef != nullptr) {
    auto *theSession = (ReflectorSession *) theSessionRef->GetObject();
    RemoveOutput(nullptr, theSession, killClients);
    (void) QTSS_Teardown(theSession->GetBroadcasterSession());
    return true;
  }
  return false;
}

void KillCommandPathInList() {
  char filePath[128] = "";
  ResizeableStringFormatter commandPath((char *) filePath, sizeof(filePath)); // ResizeableStringFormatter is safer and more efficient than StringFormatter for most paths.
  Core::MutexLocker locker(sSessionMap->GetMutex());

  for (RefHashTableIter theIter(sSessionMap->GetHashTable()); !theIter.IsDone(); theIter.Next()) {
    Ref *theRef = theIter.GetCurrent();
    if (theRef == nullptr) continue;

    commandPath.Reset();
    commandPath.Put(*(theRef->GetString()));
    commandPath.Put(sSDPKillSuffix);
    commandPath.PutTerminator();

    char *theCommandPath = commandPath.GetBufPtr();
    QTSS_Object outFileObject;
    QTSS_Error err = QTSS_OpenFileObject(theCommandPath, qtssOpenFileNoFlags, &outFileObject);
    if (err == QTSS_NoErr) {
      (void) QTSS_CloseFileObject(outFileObject);
      ::unlink(theCommandPath);
      KillSession(theRef->GetString(), true);
    }
  }
}

QTSS_Error DestroySession(QTSS_ClientSessionClosing_Params *inParams) {
  RTPSessionOutput **theOutput = nullptr;
  ReflectorOutput *outputPtr = nullptr;
  ReflectorSession *theSession = nullptr;

  Core::MutexLocker locker(sSessionMap->GetMutex());

  UInt32 theLen = sizeof(theSession);
  QTSS_Error theErr = QTSS_GetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &theSession, &theLen);
  DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
            "QTSSReflectorModule.cpp:DestroySession    sClientBroadcastSessionAttr=%" _U32BITARG_ " theSession=%p err=%" _S32BITARG_ " \n",
            (UInt32)sClientBroadcastSessionAttr, theSession, theErr);

  if (theSession != nullptr) { // 推送端
    ReflectorSession *deletedSession = nullptr;
    theErr = QTSS_SetValue(inParams->inClientSession, sClientBroadcastSessionAttr, 0, &deletedSession, sizeof(deletedSession));

    SourceInfo *theSourceInfo = theSession->GetSourceInfo();
    if (theSourceInfo == nullptr) return QTSS_NoErr;

    UInt32 numStreams = theSession->GetNumStreams();
    SourceInfo::StreamInfo *theStreamInfo = nullptr;

    for (UInt32 index = 0; index < numStreams; index++) {
      theStreamInfo = theSourceInfo->GetStreamInfo(index);
      if (theStreamInfo != nullptr)
        theStreamInfo->fSetupToReceive = false;
    }

    bool killClients = false; // the pref as the default
    UInt32 theLenTemp = sizeof(killClients);
    (void) QTSS_GetValue(inParams->inClientSession, sKillClientsEnabledAttr, 0, &killClients, &theLenTemp);

    DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
              "QTSSReflectorModule.cpp:DestroySession broadcaster theSession=%p\n",
              theSession);
    theSession->RemoveSessionFromOutput(inParams->inClientSession);

    RemoveOutput(nullptr, theSession, killClients);
  } else { // 客户端
    theLen = 0;
    theErr = QTSS_GetValuePtr(inParams->inClientSession, sOutputAttr, 0, (void **) &theOutput, &theLen);
    if ((theErr != QTSS_NoErr) || (theLen != sizeof(RTPSessionOutput *)) ||
        (theOutput == nullptr) || (*theOutput == nullptr))
      return QTSS_RequestFailed;
    theSession = (*theOutput)->GetReflectorSession();

    if (theOutput != nullptr)
      outputPtr = (ReflectorOutput *) *theOutput;

    if (outputPtr != nullptr) {
      RemoveOutput(outputPtr, theSession, false);
      RTPSessionOutput *theOutput2 = nullptr;
      (void) QTSS_SetValue(inParams->inClientSession, sOutputAttr, 0, &theOutput2, sizeof(theOutput2));
    }

  }

  return QTSS_NoErr;
}

void RemoveOutput(ReflectorOutput *inOutput, ReflectorSession *theSession, bool killClients) {
  // 对ReflectorSession的引用继续处理,包括推送端和客户端
  Assert(theSession);
  if (theSession != nullptr) {
    if (inOutput != nullptr) {
      // ReflectorSession移除客户端
      theSession->RemoveOutput(inOutput, true);
    } else {
      // 推送端
      SourceInfo *theInfo = theSession->GetSourceInfo();
      Assert(theInfo);

      //if (theInfo->IsRTSPControlled()) {
      //    FileDeleter(theSession->GetSourceID());
      //}

      if (killClients || sTearDownClientsOnDisconnect) {
        theSession->TearDownAllOutputs();
      }
    }
    // 检测推送端或者客户端退出时,ReflectorSession是否需要退出
    Ref *theSessionRef = theSession->GetRef();
    if (theSessionRef != nullptr) {
      DEBUG_LOG(DEBUG_REFLECTOR_MODULE,
                "QTSSReflectorModule.cpp:RemoveOutput UnRegister session =%p refcount=%" _U32BITARG_ "\n",
                theSessionRef, theSessionRef->GetRefCount()) ;

      if (theSessionRef->GetRefCount() > 0)
        sSessionMap->Release(theSessionRef);

      DEBUG_LOG(DEBUG_REFLECTOR_SESSION,
                "QTSSReflectorModule.cpp:RemoveOutput Session =%p refcount=%" _U32BITARG_ "\n",
                theSession->GetRef(), theSession->GetRef()->GetRefCount());

      if (theSessionRef->GetRefCount() == 0) {

        DEBUG_LOG(DEBUG_REFLECTOR_SESSION,
                  "QTSSReflectorModule.cpp:RemoveOutput UnRegister and delete session =%p refcount=%" _U32BITARG_ "\n",
                  theSessionRef, theSessionRef->GetRefCount());

        sSessionMap->UnRegister(theSessionRef);
        //delete theSession;
        SDPCache::GetInstance()->eraseSdpMap(theSession->GetSourceID()->Ptr);
        theSession->DelRedisLive();

        theSession->Signal(Thread::Task::kKillEvent);
      }
    }
  }
  delete inOutput;
}

bool AcceptSession(QTSS_StandardRTSP_Params *inParams) {
  QTSS_RTSPSessionObject inRTSPSession = inParams->inRTSPSession;
  QTSS_RTSPRequestObject theRTSPRequest = inParams->inRTSPRequest;

  QTSS_ActionFlags action = QTSSModuleUtils::GetRequestActions(theRTSPRequest);
  if (action != qtssActionFlagsWrite)
    return false;

  if (QTSSModuleUtils::UserInGroup(
      QTSSModuleUtils::GetUserProfileObject(theRTSPRequest), sBroadcasterGroup.Ptr, sBroadcasterGroup.Len))
    return true; // ok we are allowing this broadcaster user

  char remoteAddress[20] = {0};
  StrPtrLen theClientIPAddressStr(remoteAddress, sizeof(remoteAddress));
  QTSS_Error err = QTSS_GetValue(
      inRTSPSession, qtssRTSPSesRemoteAddrStr, 0, (void *) theClientIPAddressStr.Ptr, &theClientIPAddressStr.Len);
  if (err != QTSS_NoErr)
    return false;

  if (IPComponentStr(&theClientIPAddressStr).IsLocal()) {
    return !sAuthenticateLocalBroadcast;
  }

  return QTSSModuleUtils::AddressInList(sPrefs, sIPAllowListID, &theClientIPAddressStr);
}

QTSS_Error ReflectorAuthorizeRTSPRequest(QTSS_StandardRTSP_Params *inParams) {
  if (AcceptSession(inParams)) {
    bool allowed = true;
    QTSS_RTSPRequestObject request = inParams->inRTSPRequest;
    (void) QTSSModuleUtils::AuthorizeRequest(request, &allowed, &allowed, &allowed);
    return QTSS_NoErr;
  }

  bool allowNoAccessFiles = false;
  QTSS_ActionFlags noAction = ~qtssActionFlagsWrite; //no action anything but a write
  QTSS_ActionFlags authorizeAction = QTSSModuleUtils::GetRequestActions(inParams->inRTSPRequest);
  //printf("ReflectorAuthorizeRTSPRequest authorizeAction=%d qtssActionFlagsWrite=%d\n", authorizeAction, qtssActionFlagsWrite);
  bool outAllowAnyUser = false;
  bool outAuthorized = false;
  QTAccessFile accessFile;
  accessFile.AuthorizeRequest(inParams, allowNoAccessFiles, noAction, authorizeAction, &outAuthorized, &outAllowAnyUser);

  if (!outAuthorized && (authorizeAction & qtssActionFlagsWrite)) { //handle it
    //printf("ReflectorAuthorizeRTSPRequest SET not allowed\n");
    bool allowed = false;
    (void) QTSSModuleUtils::AuthorizeRequest(inParams->inRTSPRequest, &allowed, &allowed, &allowed);
  }
  return QTSS_NoErr;
}

QTSS_Error RedirectBroadcast(QTSS_StandardRTSP_Params *inParams) {
  QTSS_RTSPRequestObject theRequest = inParams->inRTSPRequest;

  char *requestPathStr;
  (void) QTSS_GetValueAsString(theRequest, qtssRTSPReqFilePath, 0, &requestPathStr);
  QTSSCharArrayDeleter requestPathStrDeleter(requestPathStr);
  StrPtrLen theRequestPath(requestPathStr);
  StringParser theRequestPathParser(&theRequestPath);

  // request path begins with a '/' for ex. /mysample.mov or /redirect_broadcast_keyword/mysample.mov
  theRequestPathParser.Expect(kPathDelimiterChar);

  StrPtrLen theFirstPath;
  theRequestPathParser.ConsumeUntil(&theFirstPath, kPathDelimiterChar);
  Assert(theFirstPath.Len != 0);

  // If the redirect_broadcast_keyword and redirect_broadcast_dir prefs are set & the first part of the path matches the keyword
  if ((sRedirectBroadcastsKeyword && sRedirectBroadcastsKeyword[0] != 0) &&
      (sBroadcastsRedirectDir && sBroadcastsRedirectDir[0] != 0) &&
      theFirstPath.EqualIgnoreCase(sRedirectBroadcastsKeyword, ::strlen(sRedirectBroadcastsKeyword))) {
    // set qtssRTSPReqRootDir
    (void) QTSS_SetValue(theRequest, qtssRTSPReqRootDir, 0, sBroadcastsRedirectDir, ::strlen(sBroadcastsRedirectDir));

    // set the request file path to the new path with the keyword stripped
    StrPtrLen theStrippedRequestPath;
    theRequestPathParser.ConsumeLength(&theStrippedRequestPath, theRequestPathParser.GetDataRemaining());
    (void) QTSS_SetValue(theRequest, qtssRTSPReqFilePath, 0, theStrippedRequestPath.Ptr, theStrippedRequestPath.Len);
  }

  return QTSS_NoErr;
}

bool AllowBroadcast(QTSS_RTSPRequestObject inRTSPRequest) {
  // If reflection of broadcasts is disabled, return false
  if (!sReflectBroadcasts)
    return false;

  // If request path is not in any of the broadcast_dir paths, return false
  return InBroadcastDirList(inRTSPRequest);
}

bool InBroadcastDirList(QTSS_RTSPRequestObject inRTSPRequest) {
  //Babosa 2016.12.2
  return true;

  bool allowed = false;

  char *theURIPathStr;
  (void) QTSS_GetValueAsString(inRTSPRequest, qtssRTSPReqFilePath, 0, &theURIPathStr);
  QTSSCharArrayDeleter requestPathStrDeleter(theURIPathStr);

  char *theLocalPathStr;
  (void) QTSS_GetValueAsString(inRTSPRequest, qtssRTSPReqLocalPath, 0, &theLocalPathStr);
  StrPtrLenDel requestPath(theLocalPathStr);

  char *theRequestPathStr = nullptr;
  char *theBroadcastDirStr = nullptr;
  bool isURI = true;

  UInt32 index = 0;
  UInt32 numValues = 0;

  (void) QTSS_GetNumValues(sPrefs, sBroadcastDirListID, &numValues);

  if (numValues == 0) return true;

  while (!allowed && (index < numValues)) {
    (void) QTSS_GetValueAsString(sPrefs, sBroadcastDirListID, index, &theBroadcastDirStr);
    StrPtrLen theBroadcastDir(theBroadcastDirStr);

    if (theBroadcastDir.Len == 0) // an empty dir matches all
      return true;

    if (IsAbsolutePath(&theBroadcastDir)) {
      theRequestPathStr = theLocalPathStr;
      isURI = false;
    } else {
      theRequestPathStr = theURIPathStr;
    }

    StrPtrLen requestPath2(theRequestPathStr);
    StringParser requestPathParser(&requestPath2);
    StrPtrLen pathPrefix;
    if (isURI)
      requestPathParser.Expect(kPathDelimiterChar);
    requestPathParser.ConsumeLength(&pathPrefix, theBroadcastDir.Len);

    // if the first part of the request path matches the broadcast_dir path, return true
    if (pathPrefix.Equal(theBroadcastDir))
      allowed = true;

    (void) QTSS_Delete(theBroadcastDirStr);

    index++;
  }

  return allowed;
}

bool IsAbsolutePath(StrPtrLen *inPathPtr) {
  StringParser thePathParser(inPathPtr);

#ifdef __Win32__
  if ((thePathParser[1] == ':') && (thePathParser[2] == kPathDelimiterChar))
#else
  if (thePathParser.PeekFast() == kPathDelimiterChar)
#endif
    return true;

  return false;
}

#define easyRTSPType 0

QTSS_Error GetDeviceStream(Easy_GetDeviceStream_Params *inParams) {
  QTSS_Error theErr = QTSS_Unimplemented;

  if (inParams->inDevice && inParams->inStreamType == easyRTSPType) {

    Core::MutexLocker locker(sSessionMap->GetMutex());

    char theStreamName[QTSS_MAX_NAME_LENGTH] = {0};
    sprintf(theStreamName, "%s%s%d", inParams->inDevice, EASY_KEY_SPLITER, inParams->inChannel);

    StrPtrLen inPath(theStreamName);

    Ref *theSessionRef = sSessionMap->Resolve(&inPath);
    ReflectorSession *theSession = nullptr;

    if (theSessionRef) {
      theSession = (ReflectorSession *) theSessionRef->GetObject();
      QTSS_ClientSessionObject clientSession = theSession->GetBroadcasterSession();
      Assert(theSession != nullptr);

      if (clientSession) {
        char *theFullRequestURL = nullptr;
        (void) QTSS_GetValueAsString(clientSession, qtssCliSesFullURL, 0, &theFullRequestURL);
        QTSSCharArrayDeleter theFileNameStrDeleter(theFullRequestURL);

        if (theFullRequestURL && inParams->outUrl) {
          strcpy(inParams->outUrl, theFullRequestURL);
          theErr = QTSS_NoErr;
        }
      }
      sSessionMap->Release(theSessionRef);
    }
  }

  return theErr;
}