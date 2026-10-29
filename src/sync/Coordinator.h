#pragma once
#include "models/TaskWorkspace.h"
#include "platform/common/SyncCredentials.h"
#include "Schema.h"
#include "ProfileIndex.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSysInfo>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QElapsedTimer>

namespace daymark::sync {
struct Response {
    int status = 0;
    QByteArray bytes;
    QJsonObject object;
    QString code;
    bool ok() const {
        return status >= 200 && status < 300 && code.isEmpty();
    }
};

// One coordinator per workspace, on the GUI event loop. SQL and business facts
// remain on TaskWorkspace's worker. Every continuation is fenced on logout.
class Coordinator final : public QObject {
  public:
    Coordinator(TaskWorkspace &workspace, const QString &directory,
                SyncCredentialStore &credentials, QObject *parent = nullptr)
        : QObject(parent), workspace(workspace), credentials(credentials), network(this),
          index(directory) {
        QSettings device(directory + QStringLiteral("/sync-device.ini"), QSettings::IniFormat);
        installation = device.value("installation_id").toString();
        if (!identifier(installation)) {
            installation = uuid();
            device.setValue("installation_id", installation);
            device.sync();
        }
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, this, [this] { cycle(); });
        connect(&workspace, &TaskWorkspace::syncActionRequested, this,
                [this](const QString &action, const QVariantMap &values) {
                    if (action == "login")
                        login(values);
                    else if (action == "register")
                        login(values, true);
                    else if (action == "logout")
                        logout();
                    else if (action == "rebuild")
                        rebuild();
                    else if (action == "open_profile")
                        openProfile(values.value("id").toString());
                    else if (action == "now") {
                        timer.stop();
                        cycle();
                    } else if (action == "resolve" && !busy) {
                        busy = true;
                        db("resolve", QJsonObject::fromVariantMap(values), [this](auto r) {
                            if (!r.ok()) {
                                fail(r.error.key);
                                return;
                            }
                            profile = r.value;
                            busy = false;
                            cycle();
                        });
                    }
                });
        connect(&workspace, &TaskWorkspace::stateChanged, this, [this] {
            if (!started && this->workspace.ready()) {
                started = true;
                initialize();
            }
        });
        connect(&workspace, &TaskWorkspace::operationFinished, this,
                [this](const QString &, bool ok) {
                    if (!ok || !this->workspace.ready())
                        return;
                    db("profile", {}, [this](auto p) {
                        if (p.ok()) {
                            profile = p.value;
                            publish();
                        }
                        if (authenticated && !busy)
                            timer.start(200);
                    });
                });
        if (workspace.ready()) {
            started = true;
            initialize();
        }
    }
    ~Coordinator() override {
        ++serial;
        timer.stop();
        if (active)
            active->abort();
        tokens = {};
    }
    // Tests may inject a dedicated trusted CA. Production uses Qt's system
    // trust store and hostname verification; SSL errors are never ignored.
    void setTlsConfiguration(const QSslConfiguration &configuration) {
        tls = configuration;
    }
    static Result<QString> origin(const QString &text) {
        QUrl url(text.trimmed(), QUrl::StrictMode);
        if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() ||
            !url.query().isEmpty() || !url.fragment().isEmpty() ||
            (!url.path().isEmpty() && url.path() != "/"))
            return {{}, failure(QStringLiteral("syncOriginInvalid"))};
        const bool loopback =
            url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1";
        if (url.scheme() != "https" &&
            !(url.scheme() == "http" && loopback &&
              qEnvironmentVariableIntValue("DAYMARK_SYNC_ALLOW_LOOPBACK_HTTP") == 1))
            return {{}, failure(QStringLiteral("syncHttpsRequired"))};
        url.setPath({});
        return {url.toString(QUrl::FullyEncoded), {}};
    }

  private:
    TaskWorkspace &workspace;
    SyncCredentialStore &credentials;
    QNetworkAccessManager network;
    QPointer<QNetworkReply> active;
    QTimer timer;
    ProfileIndex index;
    QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
    QJsonObject profile, tokens, capabilities;
    QString server, installation, message, safeCode;
    bool started = false, busy = false, authenticated = false;
    quint64 serial = 0;
    int failures = 0;
    QElapsedTimer cycleTime;
    using Done = std::function<void(Response)>;
    QString base() const {
        return QStringLiteral("/v1/datasets/") + profile.value("dataset_id").toString();
    }
    QString epochQuery() const {
        return QStringLiteral("?data_epoch=") + profile.value("data_epoch").toString();
    }
    QJsonObject credentialBinding() const {
        QJsonObject binding;
        for (const auto &f : {"profile_id", "server_origin", "server_instance_id", "user_id",
                              "dataset_id", "installation_id"})
            binding.insert(QLatin1String(f), profile.value(QLatin1String(f)));
        return binding;
    }
    QJsonObject operation(const QString &id) const {
        return {{"protocol_version", 1},
                {"data_epoch", profile.value("data_epoch")},
                {"operation_id", id}};
    }
    void publish() {
        auto state = profile.toVariantMap();
        state.insert("busy", busy);
        state.insert("safe_error_code", safeCode);
        state.insert("authenticated", authenticated);
        state.insert("profiles", index.entries().toVariantList());
        state.insert("status", message.isEmpty() ? (authenticated ? tr("已连接，修改将自动同步")
                                                    : profile.value("mode") == "ACCOUNT"
                                                        ? tr("已退出，修改保留在此设备")
                                                        : tr("仅存储在此设备"))
                                                 : message);
        workspace.setSyncState(state);
    }
    void db(const QString &op, QJsonObject input, std::function<void(Result<QJsonObject>)> done) {
        const auto fence = serial;
        if (op != "profile" && op != "bind") {
            input.insert("generation", profile.value("generation"));
            input.insert("profile_id", profile.value("profile_id"));
        }
        QPointer<Coordinator> self(this);
        workspace.syncExecute(op, input, [self, fence, done = std::move(done)](auto result) {
            if (!self || fence != self->serial)
                return;
            done(result);
        });
    }
    void switchDatabase(const QString &path, std::function<void(Result<void>)> done) {
        QPointer<Coordinator> self(this);
        const auto fence = serial;
        workspace.switchSyncDatabase(path, [self, fence, done = std::move(done)](auto r) {
            if (self && self->serial == fence)
                done(r);
        });
    }
    void initialize() {
        db("profile", {}, [this](auto r) {
            if (!r.ok()) {
                fail(r.error.key);
                return;
            }
            profile = r.value;
            server = profile.value("server_origin").toString();
            publish();
            if (profile.value("mode") != "ACCOUNT" ||
                profile.value("auth_state") != "AUTHENTICATED")
                return;
            if (!origin(server).ok()) {
                fail("syncHttpsRequired");
                return;
            }
            const auto loaded = credentials.read(profile.value("profile_id").toString());
            if (!loaded.ok()) {
                fail(loaded.error.key);
                return;
            }
            if (loaded.value.isEmpty()) {
                authRequired();
                return;
            }
            const auto envelope = StrictJson::parse(loaded.value, 4096);
            if (!envelope.ok() ||
                envelope.value.toObject().value("credential_format_version") != 1 ||
                envelope.value.toObject().value("binding") != credentialBinding()) {
                authRequired();
                return;
            }
            const auto pair = envelope.value.toObject().value("token_pair");
            if (!Schema::accepts("TokenPair", pair)) {
                authRequired();
                return;
            }
            tokens = pair.toObject();
            authenticated = true;
            busy = true;
            publish();
            revokePending([this] {
                busy = false;
                publish();
                timer.start(0);
            });
        });
    }
    void request(const QByteArray &method, const QString &path, const QByteArray &data,
                 const QString &schema, Done done, bool authorized = true, bool retried = false,
                 const QByteArray &chunkHash = {}, const QByteArray &accessOverride = {}) {
        if (active) {
            fail(QStringLiteral("syncRequestOverlap"));
            return;
        }
        if (cycleTime.isValid() && cycleTime.elapsed() > 300000) {
            fail(QStringLiteral("syncTimeout"));
            return;
        }
        QNetworkRequest req(QUrl(server + path));
        req.setSslConfiguration(tls);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
        req.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
        req.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
        req.setTransferTimeout(path == "/v1/auth/logout" ? 1000 : 15000);
        req.setRawHeader("X-Daymark-DTO-Version", "1");
        req.setRawHeader("Accept", "application/json");
        if (authorized)
            req.setRawHeader("Authorization",
                             "Bearer " + (accessOverride.isEmpty()
                                              ? tokens.value("access_token").toString().toLatin1()
                                              : accessOverride));
        if (method != "GET" && method != "DELETE")
            req.setHeader(QNetworkRequest::ContentTypeHeader,
                          chunkHash.isEmpty() ? "application/json" : "application/octet-stream");
        if (!chunkHash.isEmpty())
            req.setRawHeader("X-Chunk-SHA256", chunkHash);
        auto *reply = network.sendCustomRequest(req, method, data);
        active = reply;
        const auto fence = serial;
        auto bytes = std::make_shared<QByteArray>();
        auto tooLarge = std::make_shared<bool>(false);
        connect(reply, &QNetworkReply::readyRead, this, [reply, bytes, tooLarge] {
            if (bytes->size() + reply->bytesAvailable() > 64 * 1024 * 1024) {
                *tooLarge = true;
                reply->abort();
                return;
            }
            *bytes += reply->readAll();
        });
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, bytes, tooLarge, fence, method, path, data, schema,
                 done = std::move(done), authorized, retried, chunkHash]() mutable {
                    *bytes += reply->readAll();
                    Response result;
                    result.status =
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    result.bytes = *bytes;
                    if (active == reply)
                        active = nullptr;
                    const auto networkError = reply->error();
                    reply->deleteLater();
                    if (fence != serial)
                        return;
                    if (*tooLarge)
                        result.code = "syncPayloadTooLarge";
                    else if (result.status == 0 ||
                             (networkError != QNetworkReply::NoError && result.status < 400))
                        result.code = "syncNetworkError";
                    if (result.status == 401 && authorized && !retried && !tokens.isEmpty()) {
                        refresh([this, method, path, data, schema, done, chunkHash] {
                            request(method, path, data, schema, done, true, true, chunkHash);
                        });
                        return;
                    }
                    if (result.status >= 400) {
                        auto problem = Schema::decode("Problem", result.bytes, 1024 * 1024);
                        result.code = problem.ok() ? problem.value.value("code").toString()
                                                   : QStringLiteral("syncProtocolMismatch");
                    } else if (result.code.isEmpty() && !schema.isEmpty()) {
                        auto decoded = Schema::decode(schema, result.bytes);
                        if (!decoded.ok())
                            result.code = decoded.error.key;
                        else
                            result.object = decoded.value;
                    }
                    done(result);
                });
    }
    void refresh(std::function<void()> resumed) {
        const auto previous = tokens;
        request(
            "POST", "/v1/auth/refresh",
            json({{"protocol_version", 1}, {"refresh_token", previous.value("refresh_token")}}),
            "TokenPair",
            [this, resumed](Response r) {
                if (!r.ok()) {
                    authRequired();
                    return;
                }
                if (!saveTokens(r.object)) {
                    authRequired();
                    return;
                }
                resumed();
            },
            false);
    }
    bool saveTokens(const QJsonObject &pair) {
        const auto saved = credentials.write(profile.value("profile_id").toString(),
                                             json({{"credential_format_version", 1},
                                                   {"binding", credentialBinding()},
                                                   {"token_pair", pair}}));
        if (!saved.ok()) {
            message = tr("系统凭据存储不可用，请解锁后重新登录。待同步修改已保留。");
            publish();
            return false;
        }
        const auto remembered = index.remember(workspace.databasePath(), profile);
        if (!remembered.ok()) {
            message = tr("未能保存资料入口，请检查数据目录权限。");
            publish();
            return false;
        }
        tokens = pair;
        return true;
    }
    void login(const QVariantMap &values, bool registering = false) {
        if (busy)
            return;
        const auto normalized = origin(values.value("origin").toString());
        if (!normalized.ok()) {
            fail(normalized.error.key);
            return;
        }
        ++serial;
        timer.stop();
        if (active)
            active->abort();
        active = nullptr;
        busy = true;
        authenticated = false;
        tokens = {};
        server = normalized.value;
        message = registering ? tr("正在注册…") : tr("正在登录…");
        cycleTime.restart();
        publish();
        request(
            "GET", "/v1/capabilities", {}, "Capabilities",
            [this, values, registering](Response r) {
                if (!r.ok()) {
                    fail(r.code);
                    return;
                }
                capabilities = r.object;
                if (!capabilities.value("supported_protocol_versions").toArray().contains(1) ||
                    !capabilities.value("supported_dto_versions").toArray().contains(1)) {
                    fail("syncProtocolMismatch");
                    return;
                }
                for (const auto &f : {"snapshots", "imports", "staged_payloads"})
                    if (!capabilities.value("features")
                             .toObject()
                             .value(QLatin1String(f))
                             .toBool()) {
                        fail("syncProtocolMismatch");
                        return;
                    }
                const QStringList commands{
                    "task.create",        "task.patch",      "task.change_state",
                    "task.move_tree",     "category.save",   "category.relocate",
                    "category.remove",    "tag.save",        "tag.remove",
                    "recurrence.replace", "recurrence.stop", "recurrence.materialize",
                    "ordering.replace"};
                for (const auto &c : commands)
                    if (!capabilities.value("command_types").toArray().contains(c)) {
                        fail("syncProtocolMismatch");
                        return;
                    }
                if (registering) {
                    if (!capabilities.value("metadata").toObject().value("registration_enabled").toBool()) {
                        fail("syncRegistrationDisabled");
                        return;
                    }
                    request("POST", "/v1/auth/register",
                            json({{"protocol_version", 1}, {"login", values.value("login").toString()},
                                  {"password", values.value("password").toString()}}), {},
                            [this, values](Response registered) {
                                if (!registered.ok()) {
                                    fail((registered.code == "syncNetworkError" || registered.code == "commit_result_unknown" || registered.code == "timeout") ? "syncRegistrationUncertain" :
                                         registered.code == "validation_failed" ? "syncRegistrationInvalid" : registered.code);
                                    return;
                                }
                                busy = false;
                                login(values);
                            }, false);
                    return;
                }
#ifdef Q_OS_MACOS
                const QString platform = QStringLiteral("macos");
#else
                const QString platform = QStringLiteral("windows");
#endif
                const auto id = profile.value("installation_id").toString(installation);
                request(
                    "POST", "/v1/auth/login",
                    json({{"protocol_version", 1},
                          {"login", values.value("login").toString()},
                          {"password", values.value("password").toString()},
                          {"installation_id", id},
                          {"device_name", QSysInfo::prettyProductName().left(128)},
                          {"platform", platform}}),
                    "TokenPair",
                    [this, id](Response r) {
                        if (!r.ok()) {
                            fail(r.code);
                            return;
                        }
                        tokens = r.object;
                        request("GET", "/v1/me", {}, "Me", [this, id](Response me) {
                            if (!me.ok()) {
                                fail(me.code);
                                return;
                            }
                            selectIdentity(me.object, [this, id, me] {
                                db("bind",
                                   {{"origin", server},
                                    {"me", me.object},
                                    {"capabilities", capabilities},
                                    {"installation_id", id},
                                    {"label", me.object.value("login")},
                                    {"session_id", tokens.value("session_id")}},
                                   [this](auto bound) {
                                       if (!bound.ok()) {
                                           fail(bound.error.key);
                                           return;
                                       }
                                       profile = bound.value;
                                       if (!saveTokens(tokens)) {
                                           authRequired();
                                           return;
                                       }
                                       authenticated = true;
                                       busy = false;
                                       failures = 0;
                                       if (profile.value("phase") == "RECONCILE_REQUIRED") {
                                           message = tr("同步基线已变化。重新合并会先备份本地资料，"
                                                        "并让你处理不同版本。");
                                           publish();
                                           return;
                                       }
                                       message.clear();
                                       busy = true;
                                       publish();
                                       revokePending([this] {
                                           busy = false;
                                           cycle();
                                       });
                                   });
                            });
                        });
                    },
                    false);
            },
            false);
    }
    void logout() {
        const auto access = tokens.value("access_token").toString().toLatin1();
        const auto session = tokens.value("session_id").toString();
        ++serial;
        timer.stop();
        if (active)
            active->abort();
        active = nullptr;
        authenticated = false;
        busy = true;
        const auto removed = credentials.remove(profile.value("profile_id").toString());
        tokens = {};
        cycleTime.invalidate();
        message = tr("正在退出…");
        publish();
        db("logout", {{"session_id", session}}, [this, removed, access, session](auto r) {
            if (!r.ok()) {
                fail(r.error.key);
                return;
            }
            profile = r.value;
            auto finish = [this, removed](bool revoked) {
                busy = false;
                message = !removed.ok() ? tr("已停止同步；系统凭据删除失败，请解锁后重试退出。")
                          : revoked     ? tr("已退出，修改保留在此设备")
                                        : tr("已退出此设备；旧服务器会话将在下次登录时撤销。");
                publish();
            };
            if (access.isEmpty()) {
                finish(false);
                return;
            }
            request(
                "POST", "/v1/auth/logout", {}, {},
                [this, session, finish](Response response) {
                    if (!response.ok()) {
                        finish(false);
                        return;
                    }
                    db("revoked", {{"session_id", session}},
                       [finish](auto saved) { finish(saved.ok()); });
                },
                true, true, {}, access);
        });
    }
    void revokePending(std::function<void()> done) {
        db("revocations", {}, [this, done](auto pending) {
            if (!pending.ok()) {
                fail(pending.error.key);
                return;
            }
            const auto ids = pending.value.value("session_ids").toArray();
            if (ids.isEmpty()) {
                done();
                return;
            }
            const auto session = ids.first().toString();
            request("DELETE", "/v1/devices/" + session, {}, {},
                    [this, session, done](Response response) {
                        if (!response.ok() && response.status != 404) {
                            fail(response.code);
                            return;
                        }
                        db("revoked", {{"session_id", session}}, [this, done](auto saved) {
                            if (!saved.ok()) {
                                fail(saved.error.key);
                                return;
                            }
                            revokePending(done);
                        });
                    });
        });
    }
    void rebuild() {
        if (busy || !authenticated || profile.value("phase") != "RECONCILE_REQUIRED")
            return;
        busy = true;
        message = tr("正在备份并重新合并…");
        cycleTime.restart();
        publish();
        request("GET", "/v1/me", {}, "Me", [this](Response me) {
            if (!me.ok()) {
                fail(me.code);
                return;
            }
            QJsonObject dataset;
            for (const auto &x : me.object.value("datasets").toArray())
                if (x.toObject().value("id") == profile.value("dataset_id"))
                    dataset = x.toObject();
            db("rebuild", {{"dataset", dataset}}, [this](auto r) {
                if (!r.ok()) {
                    fail(r.error.key);
                    return;
                }
                profile = r.value;
                busy = false;
                cycle();
            });
        });
    }
    void selectIdentity(const QJsonObject &me, std::function<void()> selected) {
        if (profile.value("mode") != "ACCOUNT" ||
            (profile.value("server_origin") == server &&
             profile.value("server_instance_id") == capabilities.value("server_instance_id") &&
             profile.value("user_id") == me.value("user_id"))) {
            selected();
            return;
        }
        const auto remembered = index.remember(workspace.databasePath(), profile);
        if (!remembered.ok()) {
            fail(remembered.error.key);
            return;
        }
        const auto target =
            index.pathFor(server, capabilities.value("server_instance_id").toString(),
                          me.value("user_id").toString());
        const auto removed = credentials.remove(profile.value("profile_id").toString());
        if (!removed.ok()) {
            fail(removed.error.key);
            return;
        }
        db("logout", {}, [this, target, selected](auto signedOut) {
            if (!signedOut.ok()) {
                fail(signedOut.error.key);
                return;
            }
            profile = signedOut.value;
            const auto fence = serial;
            switchDatabase(target, [this, fence, selected](auto switched) {
                if (fence != serial)
                    return;
                if (!switched.ok()) {
                    fail(switched.error.key);
                    return;
                }
                db("profile", {}, [this, selected](auto loaded) {
                    if (!loaded.ok()) {
                        fail(loaded.error.key);
                        return;
                    }
                    profile = loaded.value;
                    selected();
                });
            });
        });
    }
    void openProfile(const QString &id) {
        if (busy || workspace.busy())
            return;
        const auto target = index.selectedPath(id);
        if (target.isEmpty() || target == workspace.databasePath())
            return;
        ++serial;
        timer.stop();
        authenticated = false;
        tokens = {};
        busy = true;
        publish();
        auto switchNow = [this, target] {
            const auto fence = serial;
            switchDatabase(target, [this, fence](auto result) {
                if (fence != serial)
                    return;
                if (!result.ok()) {
                    fail(result.error.key);
                    return;
                }
                db("profile", {}, [this](auto loaded) {
                    if (!loaded.ok()) {
                        fail(loaded.error.key);
                        return;
                    }
                    profile = loaded.value;
                    server = profile.value("server_origin").toString();
                    const auto remembered = index.remember(workspace.databasePath(), profile);
                    if (!remembered.ok()) {
                        fail(remembered.error.key);
                        return;
                    }
                    busy = false;
                    message = tr("已打开本地资料；登录后继续同步。");
                    publish();
                });
            });
        };
        if (profile.value("mode") == "ACCOUNT") {
            const auto removed = credentials.remove(profile.value("profile_id").toString());
            if (!removed.ok()) {
                fail(removed.error.key);
                return;
            }
            db("logout", {}, [this, switchNow](auto r) {
                if (!r.ok()) {
                    fail(r.error.key);
                    return;
                }
                profile = r.value;
                switchNow();
            });
        } else
            switchNow();
    }
    void authRequired() {
        authenticated = false;
        tokens = {};
        timer.stop();
        db("auth_required", {}, [this](auto r) {
            if (r.ok())
                profile = r.value;
            busy = false;
            message = tr("登录已失效，请重新登录。待同步修改已保留。");
            publish();
        });
    }
    void fail(const QString &code) {
        safeCode = code;
        busy = false;
        if (code == "data_epoch_mismatch" || code == "epoch_changed" || code == "cursor_expired" ||
            code == "cursor_epoch_mismatch" || code == "snapshot_expired" ||
            code == "job_state_conflict") {
            const bool keepSession = code == "snapshot_expired" || code == "job_state_conflict";
            timer.stop();
            db("reconcile", {}, [this, keepSession](auto r) {
                if (!r.ok()) {
                    safeCode = r.error.key;
                    message =
                        tr("同步恢复前的备份未完成。原资料仍保留，请检查磁盘空间和权限后重试。");
                    publish();
                    return;
                }
                profile = r.value;
                if (!keepSession)
                    authenticated = false;
                message =
                    keepSession
                        ? tr("传输作业已失效。原资料和待传修改已备份，请选择“备份并重新合并”。")
                        : tr("服务器已恢复或同步记录已过期。原资料已备份，请重新登录后重新合并。");
                publish();
            });
            return;
        }
        if (code == "syncRegistrationDisabled")
            message = tr("此服务器未开放注册，请联系管理员或登录已有账号。");
        else if (code == "syncRegistrationUncertain")
            message = tr("未收到注册结果。账号可能已创建，请先尝试登录；本地资料仍保留。");
        else if (code == "syncRegistrationInvalid")
            message = tr("请使用3～254位英文字母、数字或 . _ @ + - 作为账号；密码至少15个字符，最多1024字节。");
        else if (code == "rate_limited")
            message = tr("请求过于频繁，请稍后重试。");
        else if (code == "invalid_credentials")
            message = tr("账号或密码不正确。");
        else if (code == "syncProfileMismatch")
            message = tr("此资料属于其他账号或服务器，请打开该账号的独立本地资料后登录。");
        else if (code == "syncHttpsRequired")
            message = tr("服务器地址必须使用 HTTPS。");
        else if (code == "syncOriginInvalid")
            message = tr("请输入服务器根地址，例如 https://sync.example.com。");
        else if (code == "syncCredentialsLocked" || code == "syncCredentialsUnavailable")
            message = tr("无法访问系统凭据存储，请解锁后重新登录。");
        else if (code.startsWith("syncLegacy") || code == "syncInitialIdentityConflict")
            message = tr("现有资料需要先处理兼容性或同 ID 数据冲突，原资料已保留。");
        else if (code == "syncProtocolMismatch")
            message = tr("客户端与服务器接口不兼容，请检查版本。");
        else
            message = tr("同步暂未完成，修改已保留。检查网络后可重试。");
        publish();
        if (authenticated) {
            const int delay = qMin(60000, 1000 * (1 << qMin(failures++, 5))) +
                              QRandomGenerator::global()->bounded(400);
            timer.start(delay);
        }
    }
    void cycle() {
        if (busy || !authenticated || !workspace.ready() ||
            profile.value("phase") == "RECONCILE_REQUIRED")
            return;
        if (workspace.syncApplyPaused()) {
            timer.start(500);
            return;
        }
        busy = true;
        message = tr("正在同步…");
        cycleTime.restart();
        publish();
        db("profile", {}, [this](auto r) {
            if (!r.ok()) {
                fail(r.error.key);
                return;
            }
            profile = r.value;
            if (profile.value("phase") == "BOOTSTRAP") {
                bootstrap();
                return;
            }
            if (profile.value("phase") != "ACTIVE") {
                busy = false;
                publish();
                return;
            }
            pull([this] { pushNext(); });
        });
    }
    void idle() {
        db("ensure_conflicts", {}, [this](auto p) {
            if (!p.ok()) {
                fail(p.error.key);
                return;
            }
            profile = p.value;
            busy = false;
            failures = 0;
            safeCode.clear();
            message = !profile.value("conflicts").toArray().isEmpty()
                          ? tr("有修改冲突，请选择保留的版本。")
                      : profile.value("blocked_count").toInt() > 0
                          ? tr("有修改需要更新客户端后同步。内容已保留，可先在“数据与备份”中导出。")
                      : profile.value("pending_count").toInt() > 0
                          ? tr("仍有修改尚未同步，原内容保留在此设备。")
                          : tr("已同步");
            publish();
            timer.start(30000);
        });
    }
    void pull(std::function<void()> done) {
        db("checkpoint", {}, [this, done](auto cp) {
            if (!cp.ok()) {
                fail(cp.error.key);
                return;
            }
            const auto cursor = cp.value.value("cursor").toString();
            if (cursor.isEmpty()) {
                fail("syncProtocolMismatch");
                return;
            }
            request(
                "GET",
                base() + "/sync/pull" + epochQuery() +
                    "&cursor=" + QString::fromLatin1(QUrl::toPercentEncoding(cursor)) + "&limit=16",
                {}, "PullResponse", [this, cursor, done](Response r) {
                    if (!r.ok()) {
                        fail(r.code);
                        return;
                    }
                    auto page = r.object;
                    auto deliveries = std::make_shared<QJsonArray>(page.value("batches").toArray());
                    applyDeliveries(deliveries, 0, cursor, [this, page, done](const QString &last) {
                        db("tail",
                           {{"cursor", page.value("next_cursor")},
                            {"expected_cursor", last},
                            {"head_seq", page.value("through_seq")}},
                           [this, page, done](auto tail) {
                               if (!tail.ok()) {
                                   fail(tail.error.key);
                                   return;
                               }
                               if (page.value("has_more").toBool()) {
                                   pull(done);
                                   return;
                               }
                               request("POST", base() + "/sync/progress",
                                       json({{"protocol_version", 1},
                                             {"data_epoch", profile.value("data_epoch")},
                                             {"applied_seq", tail.value.value("applied_seq")}}),
                                       {}, [this, done](Response r) {
                                           if (!r.ok()) {
                                               fail(r.code);
                                               return;
                                           }
                                           done();
                                       });
                           });
                    });
                });
        });
    }
    void applyDeliveries(std::shared_ptr<QJsonArray> items, int index, const QString &cursor,
                         std::function<void(QString)> done) {
        if (index == items->size()) {
            done(cursor);
            return;
        }
        const auto delivery = items->at(index).toObject(),
                   batch = delivery.value("batch").toObject();
        auto apply = [this, items, index, cursor, delivery, done](const QJsonObject &b) {
            db("batch",
               {{"batch", b},
                {"expected_cursor", cursor},
                {"after_cursor", delivery.value("after_cursor")}},
               [this, items, index, delivery, done](auto result) {
                   if (!result.ok()) {
                       fail(result.error.key);
                       return;
                   }
                   applyDeliveries(items, index + 1, delivery.value("after_cursor").toString(),
                                   done);
               });
        };
        if (batch.contains("changes"))
            apply(batch);
        else {
            const auto ref = batch.value("batch_ref").toObject();
            download(ref.value("job_id").toString(),
                     [this, apply, batch, ref](const QJsonObject &, const QByteArray &bytes) {
                         const auto decoded = Schema::decode("ChangeBatch", bytes);
                         if (!decoded.ok()) {
                             fail(decoded.error.key);
                             return;
                         }
                         if (decoded.value.value("seq") != batch.value("seq") ||
                             QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() !=
                                 ref.value("sha256").toString().toLatin1()) {
                             fail("syncHashMismatch");
                             return;
                         }
                         apply(decoded.value);
                     });
        }
    }
    void pushNext() {
        db("prepare", {}, [this](auto prepared) {
            if (!prepared.ok()) {
                fail(prepared.error.key);
                return;
            }
            if (prepared.value.isEmpty()) {
                idle();
                return;
            }
            if (prepared.value.value("kind") == "IMPORT") {
                pushImport(prepared.value);
                return;
            }
            const auto bytes = prepared.value.value("payload").toString().toUtf8();
            const auto payload = QJsonDocument::fromJson(bytes).object();
            if (bytes.size() <= 1048576 && payload.value("commands").toArray().size() <= 64 &&
                payload.value("preconditions").toObject().value("entities").toArray().size() <=
                    512) {
                pushPayload(payload, payload);
                return;
            }
            db("transport", {{"mutation_id", payload.value("mutation_id")}},
               [this, payload](auto info) {
                   if (!info.ok()) {
                       fail(info.error.key);
                       return;
                   }
                   if (info.value.value("payload_ref").isObject()) {
                       auto ref = payload;
                       ref.remove("commands");
                       ref.remove("preconditions");
                       ref.insert("payload_ref", info.value.value("payload_ref"));
                       pushPayload(ref, payload);
                       return;
                   }
                   const auto content = json({{"preconditions", payload.value("preconditions")},
                                              {"commands", payload.value("commands")}});
                   auto create = operation(info.value.value("operation_id").toString());
                   for (const auto &f : {"mutation_id", "installation_id", "client_seq"})
                       create.insert(QLatin1String(f), payload.value(QLatin1String(f)));
                   create.insert("expected_bytes", QString::number(content.size()));
                   upload("staged-mutations", create, content,
                          [this, payload](const QString &, const QJsonObject &sealed) {
                              db("transport",
                                 {{"mutation_id", payload.value("mutation_id")},
                                  {"payload_ref", sealed.value("payload_ref")}},
                                 [this, payload](auto saved) {
                                     if (!saved.ok()) {
                                         fail(saved.error.key);
                                         return;
                                     }
                                     auto ref = payload;
                                     ref.remove("commands");
                                     ref.remove("preconditions");
                                     ref.insert("payload_ref", saved.value.value("payload_ref"));
                                     pushPayload(ref, payload);
                                 });
                          });
               });
        });
    }
    void pushPayload(const QJsonObject &body, const QJsonObject &frozen) {
        const auto mid = frozen.value("mutation_id");
        request("POST", base() + "/sync/push", json(body), "Receipt", [this, mid](Response r) {
            if (!r.ok()) {
                if (r.code == "payload_ref_expired" || r.code == "job_expired") {
                    db("transport", {{"mutation_id", mid}, {"reset", true}}, [this](auto reset) {
                        if (!reset.ok()) {
                            fail(reset.error.key);
                            return;
                        }
                        pushNext();
                    });
                    return;
                }
                if (r.status == 409) {
                    db("conflict", {{"mutation_id", mid}}, [this](auto blocked) {
                        if (!blocked.ok()) {
                            fail(blocked.error.key);
                            return;
                        }
                        pull([this] { idle(); });
                    });
                } else {
                    db((r.status >= 400 && r.status < 500) ? "reject" : "retry",
                       {{"mutation_id", mid}, {"code", r.code}, {"delay_ms", 1000}},
                       [this, r](auto updated) {
                           if (!updated.ok()) {
                               fail(updated.error.key);
                               return;
                           }
                           fail(r.code);
                       });
                }
                return;
            }
            db("receipt", {{"receipt", r.object}}, [this](auto receipt) {
                if (!receipt.ok()) {
                    fail(receipt.error.key);
                    return;
                }
                pull([this] { pushNext(); });
            });
        });
    }
    void bootstrap() {
        startBootstrap();
    }
    void pushImport(const QJsonObject &prepared) {
        const auto mid = prepared.value("mutation_id").toString(),
                   operationId = stableId(QStringLiteral("import:") + mid);
        const auto bytes = prepared.value("payload").toString().toUtf8();
        const auto payload = QJsonDocument::fromJson(bytes).object();
        auto create = operation(operationId);
        create.insert("mutation_id", mid);
        create.insert("installation_id", profile.value("installation_id"));
        create.insert("client_seq", prepared.value("client_seq"));
        create.insert("expected_bytes", QString::number(bytes.size()));
        create.insert("base_head_seq", payload.value("base_head_seq"));
        create.insert("dto_version", 1);
        create.insert("source_export_version", 10);
        upload("imports", create, bytes,
               [this, mid, operationId](const QString &job, const QJsonObject &) {
                   request("POST", base() + "/imports/" + job + "/publish",
                           json(operation(operationId)), "Receipt", [this, mid, job](Response r) {
                               if (r.code == "import_rebase_required") {
                                   request("DELETE", base() + "/jobs/" + job + epochQuery(), {},
                                           "Job", [this, mid](Response cancelled) {
                                               if (!cancelled.ok()) {
                                                   fail(cancelled.code);
                                                   return;
                                               }
                                               pull([this, mid] {
                                                   db("rebase_import", {{"mutation_id", mid}},
                                                      [this](auto rebased) {
                                                          if (!rebased.ok()) {
                                                              fail(rebased.error.key);
                                                              return;
                                                          }
                                                          pushNext();
                                                      });
                                               });
                                           });
                                   return;
                               }
                               if (!r.ok()) {
                                   db(r.status == 409 ? "conflict" : "retry",
                                      {{"mutation_id", mid}, {"code", r.code}, {"delay_ms", 1000}},
                                      [this, r](auto updated) {
                                          if (!updated.ok()) {
                                              fail(updated.error.key);
                                              return;
                                          }
                                          if (r.status == 409)
                                              pull([this] { idle(); });
                                          else
                                              fail(r.code);
                                      });
                                   return;
                               }
                               db("receipt", {{"receipt", r.object}}, [this](auto receipt) {
                                   if (!receipt.ok()) {
                                       fail(receipt.error.key);
                                       return;
                                   }
                                   pull([this] { pushNext(); });
                               });
                           });
               });
    }
    void startBootstrap() {
        db("bootstrap", {}, [this](auto meta) {
            if (!meta.ok()) {
                fail(meta.error.key);
                return;
            }
            request("POST", base() + "/snapshots",
                    json(operation(meta.value.value("snapshot_operation_id").toString())), "Job",
                    [this, meta](Response r) {
                        if (!r.ok()) {
                            fail(r.code);
                            return;
                        }
                        waitReady(r.object.value("id").toString(), [this, meta](const QString &id) {
                            download(id, [this, meta, id](const QJsonObject &manifest,
                                                          const QByteArray &bytes) {
                                acceptSnapshot(meta.value, id, manifest, bytes);
                            });
                        });
                    });
        });
    }
    void acceptSnapshot(const QJsonObject &meta, const QString &id, const QJsonObject &manifest,
                        const QByteArray &bytes) {
        const auto snapshot = Schema::decode("SnapshotPayload", bytes);
        if (!snapshot.ok()) {
            fail(snapshot.error.key);
            return;
        }
        db("snapshot",
           {{"snapshot", snapshot.value}, {"resume_cursor", manifest.value("resume_cursor")}},
           [this, meta, id](auto initial) {
               if (!initial.ok()) {
                   fail(initial.error.key);
                   return;
               }
               if (initial.value.value("needs_resolution").toBool()) {
                   idle();
                   return;
               }
               db("freeze_import", {{"payload", initial.value}},
                  [this, meta, id, initial](auto frozen) {
                      if (!frozen.ok()) {
                          fail(frozen.error.key);
                          return;
                      }
                      auto create = operation(meta.value("import_operation_id").toString());
                      const auto bytes = json(initial.value);
                      create.insert("mutation_id", meta.value("mutation_id"));
                      create.insert("installation_id", profile.value("installation_id"));
                      create.insert("client_seq", meta.value("client_seq"));
                      create.insert("expected_bytes", QString::number(bytes.size()));
                      create.insert("base_head_seq", initial.value.value("base_head_seq"));
                      create.insert("dto_version", 1);
                      create.insert("source_export_version", 10);
                      upload("imports", create, bytes,
                             [this, id, meta](const QString &job, const QJsonObject &) {
                                 publishInitial(job, id,
                                                meta.value("import_operation_id").toString());
                             });
                  });
           });
    }
    void publishInitial(const QString &job, const QString &snapshotId, const QString &operationId) {
        request("POST", base() + "/imports/" + job + "/publish", json(operation(operationId)),
                "Receipt", [this, snapshotId, job](Response r) {
                    if (!r.ok()) {
                        if (r.code == "import_rebase_required") {
                            request("DELETE", base() + "/jobs/" + job + epochQuery(), {}, "Job",
                                    [this](Response cancelled) {
                                        if (!cancelled.ok()) {
                                            fail(cancelled.code);
                                            return;
                                        }
                                        db("restart_bootstrap", {}, [this](auto restarted) {
                                            if (!restarted.ok()) {
                                                fail(restarted.error.key);
                                                return;
                                            }
                                            profile = restarted.value;
                                            bootstrap();
                                        });
                                    });
                            return;
                        }
                        fail(r.code);
                        return;
                    }
                    db("receipt", {{"receipt", r.object}}, [this, snapshotId](auto receipt) {
                        if (!receipt.ok()) {
                            fail(receipt.error.key);
                            return;
                        }
                        db("initial_committed", {}, [this, snapshotId](auto ready) {
                            if (!ready.ok()) {
                                fail(ready.error.key);
                                return;
                            }
                            profile = ready.value;
                            pull([this, snapshotId] { releaseSnapshot(snapshotId); });
                        });
                    });
                });
    }
    void releaseSnapshot(const QString &id) {
        db("checkpoint", {}, [this, id](auto cp) {
            if (!cp.ok()) {
                fail(cp.error.key);
                return;
            }
            request("POST", base() + "/snapshots/" + id + "/release",
                    json({{"protocol_version", 1},
                          {"data_epoch", profile.value("data_epoch")},
                          {"applied_seq", cp.value.value("applied_seq")}}),
                    {}, [this](Response r) {
                        if (!r.ok()) {
                            fail(r.code);
                            return;
                        }
                        pushNext();
                    });
        });
    }
    void waitReady(const QString &id, std::function<void(QString)> ready, int attempt = 0) {
        if (attempt > 120) {
            fail("syncTimeout");
            return;
        }
        request("GET", base() + "/jobs/" + id + epochQuery(), {}, "Job",
                [this, id, ready, attempt](Response r) {
                    if (!r.ok()) {
                        fail(r.code);
                        return;
                    }
                    const auto state = r.object.value("state").toString();
                    if (state == "EXPIRED" || state == "CANCELLED") {
                        fail("snapshot_expired");
                        return;
                    }
                    if (state == "READY") {
                        ready(id);
                        return;
                    }
                    if (state != "BUILDING" && state != "PUBLISHING") {
                        fail(r.object.value("safe_error_code").toString("syncJobFailed"));
                        return;
                    }
                    const auto fence = serial;
                    QTimer::singleShot(250, this, [this, fence, id, ready, attempt] {
                        if (fence == serial)
                            waitReady(id, ready, attempt + 1);
                    });
                });
    }
    void download(const QString &id, std::function<void(QJsonObject, QByteArray)> ready) {
        request("GET", base() + "/jobs/" + id + "/manifest" + epochQuery(), {}, "Manifest",
                [this, id, ready](Response r) {
                    if (!r.ok()) {
                        fail(r.code);
                        return;
                    }
                    auto semantic = r.object;
                    const auto hash = semantic.take("manifest_sha256").toString();
                    semantic.remove("metadata");
                    semantic.remove("expires_at_utc");
                    semantic.remove("resume_cursor");
                    if (digest(semantic).toHex() != hash.toLatin1()) {
                        fail("syncHashMismatch");
                        return;
                    }
                    qint64 total = 0;
                    if (!counter(r.object.value("total_bytes"), &total) ||
                        total > 64 * 1024 * 1024) {
                        fail("syncPayloadTooLarge");
                        return;
                    }
                    auto bytes = std::make_shared<QByteArray>();
                    downloadChunk(id, r.object, 0, bytes, ready);
                });
    }
    void downloadChunk(const QString &id, const QJsonObject &manifest, int index,
                       std::shared_ptr<QByteArray> bytes,
                       std::function<void(QJsonObject, QByteArray)> ready) {
        const auto chunks = manifest.value("chunks").toArray();
        if (index == chunks.size()) {
            qint64 total = 0;
            if (!counter(manifest.value("total_bytes"), &total) || bytes->size() != total ||
                QCryptographicHash::hash(*bytes, QCryptographicHash::Sha256).toHex() !=
                    manifest.value("payload_sha256").toString().toLatin1()) {
                fail("syncHashMismatch");
                return;
            }
            ready(manifest, *bytes);
            return;
        }
        const auto chunk = chunks[index].toObject();
        if (chunk.value("index").toInt() != index) {
            fail("syncProtocolMismatch");
            return;
        }
        request("GET", base() + "/jobs/" + id + "/chunks/" + QString::number(index) + epochQuery(),
                {}, {}, [this, id, manifest, index, bytes, ready, chunk](Response r) {
                    qint64 count = 0;
                    if (!r.ok()) {
                        fail(r.code);
                        return;
                    }
                    if (!counter(chunk.value("byte_count"), &count) || count > 262144 ||
                        r.bytes.size() != count ||
                        QCryptographicHash::hash(r.bytes, QCryptographicHash::Sha256).toHex() !=
                            chunk.value("sha256").toString().toLatin1() ||
                        bytes->size() + count > 64 * 1024 * 1024) {
                        fail("syncHashMismatch");
                        return;
                    }
                    *bytes += r.bytes;
                    downloadChunk(id, manifest, index + 1, bytes, ready);
                });
    }
    void upload(const QString &kind, const QJsonObject &create, const QByteArray &bytes,
                std::function<void(QString, QJsonObject)> ready) {
        if (bytes.isEmpty() || bytes.size() > 64 * 1024 * 1024) {
            fail("syncPayloadTooLarge");
            return;
        }
        request("POST", base() + "/" + kind, json(create), "Job", [this, bytes, ready](Response r) {
            if (!r.ok()) {
                fail(r.code);
                return;
            }
            const auto id = r.object.value("id").toString(),
                       state = r.object.value("state").toString();
            if (state == "EXPIRED" || state == "CANCELLED") {
                fail("snapshot_expired");
                return;
            }
            if (state == "FAILED") {
                fail("job_state_conflict");
                return;
            }
            if (state == "COMMITTED") {
                ready(id, {});
                return;
            }
            if (state == "SEALED") {
                QJsonArray chunks;
                for (qsizetype offset = 0, index = 0; offset < bytes.size();
                     offset += 262144, ++index) {
                    const auto part = bytes.mid(offset, 262144);
                    chunks.append(QJsonObject{
                        {"index", index},
                        {"byte_count", QString::number(part.size())},
                        {"sha256",
                         QString::fromLatin1(
                             QCryptographicHash::hash(part, QCryptographicHash::Sha256).toHex())}});
                }
                auto saved = std::make_shared<QJsonArray>(chunks);
                uploadChunk(id, bytes, int(chunks.size()), saved, ready);
                return;
            }
            auto chunks = std::make_shared<QJsonArray>();
            uploadChunk(id, bytes, 0, chunks, ready);
        });
    }
    void uploadChunk(const QString &id, const QByteArray &bytes, int index,
                     std::shared_ptr<QJsonArray> chunks,
                     std::function<void(QString, QJsonObject)> ready) {
        const auto part = bytes.mid(qsizetype(index) * 262144, 262144);
        if (part.isEmpty()) {
            request(
                "POST", base() + "/jobs/" + id + "/seal",
                json({{"protocol_version", 1},
                      {"data_epoch", profile.value("data_epoch")},
                      {"chunks", *chunks},
                      {"total_bytes", QString::number(bytes.size())},
                      {"payload_sha256",
                       QString::fromLatin1(
                           QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())}}),
                "Sealed", [this, id, ready](Response r) {
                    if (!r.ok()) {
                        fail(r.code);
                        return;
                    }
                    ready(id, r.object);
                });
            return;
        }
        const auto hash = QCryptographicHash::hash(part, QCryptographicHash::Sha256).toHex();
        request(
            "PUT", base() + "/jobs/" + id + "/chunks/" + QString::number(index) + epochQuery(),
            part, "Chunk",
            [this, id, bytes, index, chunks, ready, hash, part](Response r) {
                if (!r.ok()) {
                    fail(r.code);
                    return;
                }
                chunks->append(QJsonObject{{"index", index},
                                           {"byte_count", QString::number(part.size())},
                                           {"sha256", QString::fromLatin1(hash)}});
                uploadChunk(id, bytes, index + 1, chunks, ready);
            },
            true, false, hash);
    }
};
} // namespace daymark::sync
