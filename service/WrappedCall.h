#pragma once

#include <QMessageBox>
#include <QString>
#include <grpcpp/client_context.h>
#include "Message/Request.pb.h"
#include "RpcCall.h"
#include "CachedProtoData.h"

class WrappedCall {
public:
    template<typename T>
    static void setCommonRequestField(T&type) {
        type.mutable_metainfo()->set_apiversion(RpcCall::ApiVersion);
        auto* userInfo = type.mutable_userverifyinfo();
        userInfo->set_username(CachedProtoData::getInstance().UserName);
        userInfo->set_usertoken(CachedProtoData::getInstance().UserToken);
    }

    static bool defaultErrorHandler(const std::string&actionName, const grpc::Status&status,
                                    const proto::ResponseMetaInfoV1&rspMeta,
                                    QWidget* parent) {
        if (status.ok()) {
            if (rspMeta.status()) {
                return true;
            }
            QMessageBox::critical(parent, "Info", QString::fromStdString(actionName + " Failed!" + rspMeta.message()));
            return false;
        }
        QMessageBox::critical(parent, "Error", QString::fromStdString(status.error_message()));
        return false;
    }

    static bool getAllProjectMetaInfo(proto::GetAllProjectResponse&response, QWidget* parent) {
        proto::GetAllProjectRequest request;
        setCommonRequestField(request);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetAllProject(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getAllSwcMetaInfo(proto::GetAllSwcMetaInfoResponse&response, QWidget* parent) {
        proto::GetAllSwcMetaInfoRequest request;
        setCommonRequestField(request);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetAllSwcMetaInfo(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getAllDailyStatisticsMetaInfo(proto::GetAllDailyStatisticsResponse&response, QWidget* parent) {
        proto::GetAllDailyStatisticsRequest request;
        setCommonRequestField(request);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetAllDailyStatistics(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getAllUserMetaInfo(proto::GetAllUserResponse&response, QWidget* parent) {
        proto::GetAllUserRequest request;
        setCommonRequestField(request);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetAllUser(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getProjectMetaInfoByName(const std::string&projectName, proto::GetProjectResponse&response,
                                         QWidget* parent) {
        proto::GetProjectRequest request;
        setCommonRequestField(request);
        request.set_projectname(projectName);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetProject(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getSwcMetaInfoByName(const std::string&swcName, proto::GetSwcMetaInfoResponse&response,
                                     QWidget* parent) {
        proto::GetSwcMetaInfoRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetSwcMetaInfo(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getDailyStatisticsmMetaInfoByName(const std::string&dailyStatisticsName,
                                                  proto::GetDailyStatisticsResponse&response, QWidget* parent) {
        proto::GetDailyStatisticsRequest request;
        setCommonRequestField(request);
        request.set_dailystatisticsname(dailyStatisticsName);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetDailyStatistics(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getSwcFullNodeData(const std::string&swcName, proto::GetSwcFullNodeDataResponse&response,
                                   QWidget* parent) {
        proto::GetSwcFullNodeDataRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetSwcFullNodeData(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getSwcSnapshot(const std::string&swcSnapshotCollectioNname, proto::GetSnapshotResponse&response,
                               QWidget* parent) {
        proto::GetSnapshotRequest request;
        setCommonRequestField(request);
        request.set_swcsnapshotcollectionname(swcSnapshotCollectioNname);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetSnapshot(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getSwcNodeDataListByTimeAndUserResponse(const std::string&swcName, const std::string&userName,
                                                        google::protobuf::Timestamp&startTime,
                                                        google::protobuf::Timestamp&endTime,
                                                        proto::GetSwcNodeDataListByTimeAndUserResponse&response,
                                                        QWidget* parent) {
        proto::GetSwcNodeDataListByTimeAndUserRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);
        request.set_username(userName);
        request.mutable_starttime()->CopyFrom(startTime);
        request.mutable_endtime()->CopyFrom(endTime);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetSwcNodeDataListByTimeAndUser(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool addSwcNodeData(const std::string&swcName, proto::SwcDataV1&swcData,
                               proto::CreateSwcNodeDataResponse&response, QWidget* parent) {
        proto::CreateSwcNodeDataRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);
        request.mutable_swcdata()->CopyFrom(swcData);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->CreateSwcNodeData(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool modifySwcNodeData(const std::string&swcName, proto::SwcDataV1&swcData,
                                  proto::UpdateSwcNodeDataResponse&response, QWidget* parent) {
        proto::UpdateSwcNodeDataRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);
        request.mutable_swcdata()->CopyFrom(swcData);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->UpdateSwcNodeData(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool deleteSwcNodeData(const std::string&swcName, proto::SwcDataV1&swcData,
                                  proto::DeleteSwcNodeDataResponse&response, QWidget* parent) {
        proto::DeleteSwcNodeDataRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);
        request.mutable_swcdata()->CopyFrom(swcData);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->DeleteSwcNodeData(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool createSwcMeta(const std::string&name, const std::string&description, proto::CreateSwcResponse&response,
                              QWidget* parent) {
        proto::CreateSwcRequest request;
        setCommonRequestField(request);
        request.mutable_swcinfo()->set_name(name);
        request.mutable_swcinfo()->set_description(description);
        request.mutable_swcinfo()->set_swctype("eswc"); // by default when creating new swc using eswc type

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->CreateSwc(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getAllSwcIncrementRecord(const std::string&name,
                                         proto::GetAllIncrementOperationMetaInfoResponse&response,
                                         QWidget* parent) {
        proto::GetAllIncrementOperationMetaInfoRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetAllIncrementOperationMetaInfo(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getSwcIncrementRecord(const std::string&name,
                                      proto::GetIncrementOperationResponse&response,
                                      QWidget* parent) {
        proto::GetIncrementOperationRequest request;
        setCommonRequestField(request);
        request.set_incrementoperationcollectionname(name);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetIncrementOperation(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getSwcAttachmentAno(const std::string&swcName, const std::string&attachmentUuid,
                                    proto::GetSwcAttachmentAnoResponse&response,
                                    QWidget* parent) {
        proto::GetSwcAttachmentAnoRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);
        request.set_anoattachmentuuid(attachmentUuid);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetSwcAttachmentAno(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool createSwcAttachmentAno(const std::string&name, const std::string&apoFileName,
                                       const std::string&swcFileName,
                                       proto::CreateSwcAttachmentAnoResponse&response,
                                       QWidget* parent) {
        proto::CreateSwcAttachmentAnoRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);
        request.mutable_swcattachmentano()->set_apofile(apoFileName);
        request.mutable_swcattachmentano()->set_swcfile(swcFileName);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->CreateSwcAttachmentAno(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool updateSwcAttachmentAno(const std::string&name, const std::string&attachmentUuid,
                                       const std::string&apoFileName, const std::string&swcFileName,
                                       proto::UpdateSwcAttachmentAnoResponse&response,
                                       QWidget* parent) {
        proto::UpdateSwcAttachmentAnoRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);
        request.set_anoattachmentuuid(attachmentUuid);
        request.mutable_newswcattachmentano()->set_apofile(apoFileName);
        request.mutable_newswcattachmentano()->set_swcfile(swcFileName);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->UpdateSwcAttachmentAno(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool deleteSwcAttachmentAno(const std::string&name, const std::string&attachmentUuid,
                                       proto::DeleteSwcAttachmentAnoResponse&response,
                                       QWidget* parent) {
        proto::DeleteSwcAttachmentAnoRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);
        request.set_anoattachmentuuid(attachmentUuid);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->DeleteSwcAttachmentAno(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool getSwcAttachmentApo(const std::string&swcName, const std::string&attachmentUuid,
                                    proto::GetSwcAttachmentApoResponse&response,
                                    QWidget* parent) {
        proto::GetSwcAttachmentApoRequest request;
        setCommonRequestField(request);
        request.set_swcname(swcName);
        request.set_apoattachmentuuid(attachmentUuid);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetSwcAttachmentApo(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool createSwcAttachmentApo(const std::string&name, std::vector<proto::SwcAttachmentApoV1> attachments,
                                       proto::CreateSwcAttachmentApoResponse&response,
                                       QWidget* parent) {
        proto::CreateSwcAttachmentApoRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);
        for (auto&attachment: attachments) {
            request.add_swcattachmentapo()->CopyFrom(attachment);
        }

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->CreateSwcAttachmentApo(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool updateSwcAttachmentApo(const std::string&name, const std::string&attachmentUuid,
                                       std::vector<proto::SwcAttachmentApoV1> attachments,
                                       proto::UpdateSwcAttachmentApoResponse&response,
                                       QWidget* parent) {
        proto::UpdateSwcAttachmentApoRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);
        request.set_apoattachmentuuid(attachmentUuid);
        for (auto&attachment: attachments) {
            request.add_newswcattachmentapo()->CopyFrom(attachment);
        }

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->UpdateSwcAttachmentApo(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool deleteSwcAttachmentApo(const std::string&name, const std::string&attachmentUuid,
                                       proto::DeleteSwcAttachmentApoResponse&response,
                                       QWidget* parent) {
        proto::DeleteSwcAttachmentApoRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);
        request.set_apoattachmentuuid(attachmentUuid);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->DeleteSwcAttachmentApo(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool RevertSwcVersion(const std::string&name, google::protobuf::Timestamp& endTime,
                                 proto::RevertSwcVersionResponse&response,
                                 QWidget* parent) {
        proto::RevertSwcVersionRequest request;
        setCommonRequestField(request);
        request.set_swcname(name);
        request.mutable_versionendtime()->CopyFrom(endTime);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->RevertSwcVersion(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }

    static bool GetPermissionGroupByUuid(const std::string& uuid, proto::GetPermissionGroupResponse&response,
                                 QWidget* parent) {
        proto::GetPermissionGroupRequest request;
        setCommonRequestField(request);
        request.mutable_permissiongroup()->mutable_base()->set_uuid(uuid);

        grpc::ClientContext context;
        auto status = RpcCall::getInstance().Stub()->GetPermissionGroup(&context, request, &response);
        return defaultErrorHandler(__func__, status, response.metainfo(), parent);
    }
};
