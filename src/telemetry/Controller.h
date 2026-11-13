#pragma once
#include "Telemetry.h"
#include "models/TaskWorkspace.h"
#include "sync/Diagnostics.h"
#include "diagnostics/RuntimeLog.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QPointer>
#include <QSet>
#include <functional>

namespace daymark::telemetry {
struct Reply {int status=0;QJsonObject body;QString code;qint64 retryAfterMs=0;};
class Controller final:public QObject {
public:
    using Done=std::function<void(Reply)>;
    using Send=std::function<bool(const QByteArray&,const QString&,const QJsonObject&,Done)>;
    Controller(TaskWorkspace& workspace,Send send,std::function<void()> cancel,QObject* parent=nullptr)
        :QObject(parent),workspace(workspace),send(std::move(send)),cancel(std::move(cancel)){
        connect(&workspace,&TaskWorkspace::stateChanged,this,[this]{const auto key=this->workspace.view()+":"+this->workspace.scope();if(key!=lastView){lastView=key;record(Event::surfaceUsed(Surface::Main));}});
    }
    void bind(const QString& context,bool authenticated){
        if(context==binding && authenticated==connected)return;
        ++generation;++choice;binding=context;databasePath=workspace.databasePath();connected=authenticated;loaded=false;dbBusy=false;networkBusy=false;verified=false;initialized=false;explicitChoice=false;forceFlush=false;intent.reset();state={};lastError.clear();seenSurfaces.clear();nextSample=0;
        publish();
    }
    void setEnabled(bool enabled,bool saved){
        if(!connected)return;
        ++choice;explicitChoice=enabled;intent=enabled;state.insert("enabled",enabled);state.insert("confirmed",false);verified=false;lastError=saved?QString():QStringLiteral("telemetryLocalStorageFailed");
        if(!enabled)cancel();
        publish();applyIntent();
    }
    void retry(){if(!connected || dbBusy || networkBusy)return;forceFlush=true;verified=false;load("retry_now",{},[this]{loaded=true;initialized=true;pulse(true);});}
    void pulse(bool manual=false){
        if(!connected || dbBusy || networkBusy || binding.isEmpty())return;
        if(intent.has_value()){applyIntent();return;}
        if(!initialized){
            initialized=true;
            const auto file=Queue::path(workspace.databasePath(),binding);
            if(!QFileInfo::exists(file)){loaded=true;if(workspace.preferences().value("telemetryEnabled").toBool())workspace.setPreference("telemetryEnabled",false);publish();return;}
            load("inspect",{},[this]{loaded=true;if(!workspace.preferences().value("telemetryEnabled").toBool() && state.value("enabled").toBool()){setEnabled(false,true);return;}advance();});return;
        }
        const auto now=QDateTime::currentMSecsSinceEpoch();if(!manual && now<state.value("retry_after_ms").toDouble())return;
        if(now<state.value("server_retry_after_ms").toDouble()){publish();return;}
        if(state.value("pending").toBool() || (state.value("enabled").toBool()&&!verified)){advance();return;}
        if(allowed() && now>=nextSample && memorySampler){nextSample=now+30ll*60*1000;if(const auto value=memorySampler())record(Event::performance(*value));}
        if(allowed() && (forceFlush || manual || now-state.value("last_flush_ms").toDouble()>=15ll*60*1000))load("batch",{},[this]{const auto batch=state.value("batch").toObject();if(batch.isEmpty()){forceFlush=false;return;}transmit("POST","/telemetry/v1/batches",batch,[this,batch](const Reply& r){if(r.status==202 && r.code.isEmpty() && r.body.value("batch_id")==batch.value("batch_id") && r.body.value("accepted").toInt(-1)==batch.value("events").toArray().size()){forceFlush=false;load("ack",{{"batch_id",batch.value("batch_id")}},[]{});return;}if(r.status==403){pause(r.code);return;}if(r.status==409 || r.status==422){pause(r.code);return;}failed(r.code);});});
    }
    void record(const Event& event){
        if(!allowed() || dbBusy)return;
        if(event.kind==Event::Kind::SurfaceUsed){const int key=static_cast<int>(event.surface);if(seenSurfaces.contains(key))return;seenSurfaces.insert(key);}
        load("record",{{"event",event.payload()}},[]{});
    }
    void setMemorySampler(std::function<std::optional<qint64>()> sampler){memorySampler=std::move(sampler);}
    void setStartupMeasurement(qint64 ms){startupMeasurement=ms;recordStartup();}
private:
    TaskWorkspace& workspace;Send send;std::function<void()> cancel;
    std::function<std::optional<qint64>()> memorySampler;
    QString binding,databasePath,lastError,lastView;
    QJsonObject state;
    quint64 generation=0,choice=0;
    bool connected=false,loaded=false,initialized=false,dbBusy=false,networkBusy=false,verified=false,explicitChoice=false,forceFlush=false;
    std::optional<bool> intent;
    QSet<int> seenSurfaces;
    qint64 nextSample=0;
    qint64 retryDelay=0;
    qint64 startupMeasurement=-1;
    bool startupSent=false;
    void recordStartup(){if(!startupSent && !explicitChoice && startupMeasurement>=0 && allowed() && !dbBusy){startupSent=true;record(Event::startup(startupMeasurement));}}
    bool allowed()const{return connected && workspace.ready() && databasePath==workspace.databasePath() && loaded && verified && !intent.has_value() && !state.value("pending").toBool() && state.value("enabled").toBool() && state.value("confirmed").toBool() && workspace.preferences().value("telemetryEnabled").toBool();}
    void publish(){
        const bool enabled=connected && (intent.has_value()?*intent:state.value("enabled").toBool(workspace.preferences().value("telemetryEnabled").toBool()));
        QString status=!connected?tr("登录后可选择参与，当前不采集。"):state.value("pending").toBool()||intent.has_value()?(enabled?tr("等待服务器确认，当前尚未采集。"):tr("本机已停止，服务端撤回待连接完成。")):allowed()?tr("已开启，仅发送白名单统计至当前账号服务。"):state.value("needs_confirmation").toBool()?tr("许可已失效，需重新选择后才会采集。"):tr("使用统计已关闭。");
        if(!lastError.isEmpty())status+=tr(" 暂未完成，可稍后重试（%1）。").arg(sync::diagnosticCode(lastError));
        workspace.setTelemetryState({{"enabled",enabled},{"collecting",allowed()},{"busy",dbBusy||networkBusy},{"status",status},{"error_key",lastError}});
    }
    void load(const QString& operation,const QJsonObject& input,std::function<void()> done){
        if(dbBusy || databasePath!=workspace.databasePath())return;
        dbBusy=true;publish();const auto fence=generation,change=choice;QPointer<Controller> self(this);
        workspace.telemetryExecute(binding,operation,input,[self,fence,change,done=std::move(done)](auto result){if(!self || fence!=self->generation)return;self->dbBusy=false;if(change!=self->choice){self->applyIntent();return;}if(!result.ok()){self->lastError=result.error.key;self->verified=false;diagnostics::event("telemetry","queue.failed",{{"error_key",result.error.key},{"diagnostic_code",sync::diagnosticCode(result.error.key)}},"warning");self->publish();return;}self->state=result.value;self->publish();done();});
    }
    void applyIntent(){
        if(!intent.has_value() || dbBusy || networkBusy || !connected)return;
        const bool enabled=*intent;
        // Keep the user's intent until it is durably recorded. A failed write
        // must not turn a pending withdrawal into an apparent success.
        load("set",{{"enabled",enabled}},[this]{intent.reset();loaded=true;initialized=true;advance();});
    }
    void advance(){
        if(dbBusy || networkBusy || !connected)return;
        if(QDateTime::currentMSecsSinceEpoch()<state.value("server_retry_after_ms").toDouble())return;
        if(state.value("pending").toBool()){
            const auto body=state.value("consent_body").toObject();
            if(!body.isEmpty()){transmit("PUT","/telemetry/v1/consent",body,[this](const Reply& r){if(r.status==200 && r.code.isEmpty()){load("confirm",{{"remote",r.body}},[this]{verified=true;lastError.clear();publish();});}else if(r.status==409){load("conflict",{},[this]{lastError="telemetry_consent_conflict";publish();});}else failed(r.code);});return;}
        }else if(!state.value("enabled").toBool())return;
        transmit("GET","/telemetry/v1/consent",{},[this](const Reply& r){if(r.status==200 && r.code.isEmpty()){load("remote",{{"remote",r.body},{"allow_reconfirm",explicitChoice}},[this]{if(state.value("pending").toBool())advance();else{verified=true;lastError.clear();if(!state.value("enabled").toBool())workspace.setPreference("telemetryEnabled",false);publish();recordStartup();}});}else if(r.status==404){if(state.value("pending").toBool()&&!state.value("enabled").toBool())failed("telemetryProtocolMismatch");else pause("telemetryProtocolMismatch");}else failed(r.code);});
    }
    void transmit(const QByteArray& method,const QString& path,const QJsonObject& body,std::function<void(const Reply&)> done){
        const auto fence=generation,change=choice;QPointer<Controller> self(this);
        networkBusy=true;
        const bool accepted=send(method,path,body,[self,fence,change,done=std::move(done)](Reply reply){if(!self || fence!=self->generation)return;self->networkBusy=false;if(change!=self->choice){self->applyIntent();return;}self->retryDelay=reply.retryAfterMs;done(reply);self->publish();});
        if(!accepted)networkBusy=false;
        publish();
    }
    void failed(QString key){if(key.isEmpty())key="telemetryProtocolMismatch";lastError=key;load("retry",{{"server_retry_delay_ms",retryDelay}},[this]{publish();});}
    void pause(const QString& key){lastError=key;verified=false;workspace.setPreference("telemetryEnabled",false);load("pause",{},[this]{publish();});}
};
}
