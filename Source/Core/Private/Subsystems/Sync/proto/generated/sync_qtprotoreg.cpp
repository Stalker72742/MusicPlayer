
#include "sync.qpb.h"

#include <QtProtobuf/qprotobufregistration.h>

namespace soundlink::sync::v1 {
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarHello(qRegisterProtobufType<Hello>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarEnvelope(qRegisterProtobufType<Envelope>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarFrame(qRegisterProtobufType<Frame>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarWelcome(qRegisterProtobufType<Welcome>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarIndexBatch(qRegisterProtobufType<IndexBatch>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarIndexDone(qRegisterProtobufType<IndexDone>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarSyncRecord(qRegisterProtobufType<SyncRecord>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarTrackInfo(qRegisterProtobufType<TrackInfo>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarFileBody(qRegisterProtobufType<FileBody>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarOnlineBody(qRegisterProtobufType<OnlineBody>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarMetaBody(qRegisterProtobufType<MetaBody>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarPlaylistBody(qRegisterProtobufType<PlaylistBody>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarBlockRequest(qRegisterProtobufType<BlockRequest>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarBlockData(qRegisterProtobufType<BlockData>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarPing(qRegisterProtobufType<Ping>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarPong(qRegisterProtobufType<Pong>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarEndpoints(qRegisterProtobufType<Endpoints>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarBye(qRegisterProtobufType<Bye>);
static QtProtobuf::ProtoTypeRegistrar ProtoTypeRegistrarBeacon(qRegisterProtobufType<Beacon>);
static bool RegisterSyncProtobufTypes = [](){ qRegisterProtobufTypes(); return true; }();
} // namespace soundlink::sync::v1

