# AUTH/HTTP：账号、会话与传输详细设计

共享类型见[入口](README.md)，网络路径/错误格式见[v1协议](../../api/sync-v1.md)。本设计按第一方账号候选实现；账号注册/邮件恢复不默认开放。方法签名是开发契约记法，不是现存Go源码。

## 1. AuthService接口及依赖

```text
Login(ctx, {LoginName, Password, InstallationID, DeviceLabel, Platform}) -> TokenPair | Fault
Authenticate(ctx, rawAccessToken) -> Principal | Fault
Refresh(ctx, rawRefreshToken) -> TokenPair | Fault
Logout(ctx, Principal) -> void | Fault
Revoke(ctx, Principal, targetSessionID) -> void | Fault
ChangePassword(ctx, ReauthenticatedPrincipal, newPassword) -> void | Fault
ListDevices(ctx, Principal) -> []DeviceSession | Fault
CreateUser(ctx, AdminCapability, login, password) -> UserID | Fault
TokenPair = {AccessToken, RefreshToken, AccessExpiresAt, RefreshExpiresAt, SessionID, Generation}; user/dataset由Me取得
```

注入AccountReader、UnitOfWork、PasswordHasher、TokenSource、Clock、KdfGate、LoginRateLimiter。AdminCapability来自受控维护命令，不能从普通HTTP参数创建。会话列表只显示安全的设备标签、创建/过期/撤销状态，不返回token摘要。

登录名trim后ASCII小写，允许`[a-z0-9._@+-]`、3～254字符；它是账号名，不代表已验证邮箱。密码不trim/正规化，设计默认12～1024 UTF-8字节；管理员建号同一规则。若产品更改口令政策，应修改单一配置及边界用例，不能客户端/服务端各写一套限制。

PasswordHasher接口为`Hash(password)->PHC`、`Verify(PHC,password)->bool/error`、`NeedsRehash(PHC)->bool`。Argon2id采用独立16-byte盐、32-byte输出和版本化PHC编码；候选初值memory=64MiB/time=3/parallelism=1，目标机校准；解析输入设上限防止损坏PHC要求无限内存。真实密码算法组件单独测，业务UT用FakeHasher，不在每条逻辑UT执行昂贵KDF。[Go Argon2接口](https://pkg.go.dev/golang.org/x/crypto/argon2)

token为安全随机32-byte、无padding base64url，服务器存SHA-256摘要。access/refresh不能相同；新摘要不能复用已有current/history。冲突时有限重新生成，3次失败返回token_source_failed，不覆盖别人的会话。安全随机失败不降级伪随机。

## 2. 登录及验证算法

Login：限流/KDF准入→只读查账号及credential_version→事务外真实或dummy哈希→不符统一invalid_credentials→预生成pair/SessionID→写事务复查账号ACTIVE、口令版本未变、摘要未占用→插入session→Commit→只返回一次原始pair。哈希期间改密/停用则拒绝，不使用过期只读结果登录。Rehash若需要，在事务外计算，写时CAS旧credential_version；失败不破坏原口令。

Authenticate：验证token格式/长度→SHA-256定位current access→检查用户ACTIVE、session未撤销、auth_epoch和credential_version相符、now<access_expires→返回Principal。认证结果不得跨请求无限缓存。Write中的CheckSession再次检查账号/会话撤销、auth/口令版本与Principal期限；纯refresh导致的generation变化不撤回已经受理、仍未过期的Principal，新请求则必须用新access摘要认证。

Revoke/Logout：事务内检查实际session归属，设置revoked_ms；同一会话重复撤销幂等。改密先生成新PHC，事务增加credential_version并撤销全部现有会话；成功后重新登录。不以installation_id或用户提供的owner决定权限。仅同口令参数升级的Rehash以旧PHC及credential_version双CAS更新，不增加安全口令代次或无故撤销其他session。

## 3. 刷新状态机与提交顺序

```text
ACTIVE(current refresh) --successful refresh--> ACTIVE(generation+1,new pair)
ACTIVE(retired refresh replay) --> REVOKED
ACTIVE --logout/revoke/password change--> REVOKED
ACTIVE --absolute expiration--> EXPIRED
```

Refresh先事务外生成候选pair，再进写事务定位current hash或refresh_history：

1. current有效：检查账号/auth/credential/absolute expiry；把旧refresh摘要写history、expiry保留原绝对期限；更新current pair及generation，access期限=min(now+15min,absolute)，绝对refresh期限不滚动到无限。
2. retired且会话仍活跃：设置revoked_ms，回调返回nil并设置拒绝Outcome；**Commit完成之后**返回403 refresh_replayed。若撤销Commit失败，返回存储失败/结果不确定，不假称已经撤销。
3. 未知、已撤销或过期：401/403，不创建新session。不同用户token不能影响另一个活跃会话。

并发使用同一个旧refresh时，最多一条刷新成功；另一条若线性化在后，触发重放撤销该族。因此客户端必须single-flight刷新。刷新响应丢失不盲目旧token重试，重新认证并保留业务outbox；token响应不写普通业务receipt。安全存储写失败同样转重新认证，不保存明文或只持久化半个pair。

## 4. HTTP适配器和严格codec

```text
DecodePush(limitedReader, DatasetPath) -> ValidatedEnvelope | Fault
CanonicalDigest(ValidatedEnvelope, ResolvedPayload) -> [32]byte | Fault
CursorCodec.Issue(CursorClaims) -> opaqueToken
CursorCodec.Verify(token, AuthorizedScope, now) -> CursorClaims | Fault
ProblemMapper.Map(Fault, RequestID) -> {Status, Headers, Problem}
Router.Register(Services, Limits) -> http.Handler
```

中间件顺序：request ID→受限错误恢复/日志→受信代理与速率准入→期限/请求大小/解压后大小→路由/认证→严格DTO→用例→响应。认证初查前不能无界读取body；无权请求不能占writer。日志按固定字段白名单，不记录body/Authorization/query cursor。

codec拒绝重复键、未知写字段、非法UTF-8/孤立UTF-16 surrogate转义、尾部第二个JSON值、float整数、非规范UUID、溢出revision、空命令组及inline/payload_ref同时存在。版本/枚举/Field.Clear合法性由明确字段表处理，不能靠Go默认Unmarshal将缺失/null都映射零值或把非法surrogate替换字符后继续。HTTP handler不拼SQL、不决定树联动。

## 5. 规范摘要的确定性算法

为避免Go/Qt序列化差异，摘要基于验证后的类型化语义树，不基于收到的JSON字节。将path中的dataset和规范envelope合成对象；排除token/request ID/压缩/payload_ref的传输身份，引用展开为原preconditions+commands。对象键按UTF-8字节升序；实体/结构preconditions按(type/id或kind/id)排序且拒绝重复；tag_ids等声明为集合的数组排序且拒绝重复，commands及ordering序列保留顺序。文本保持原Unicode，不自动NFC处理。

规范编码为：null=`00`；false=`01`；true=`02`；string=`03 + u32大端UTF-8字节数 + 字节`；int64=`04 + 8字节大端二补码`；object=`05 + u32成员数 + 各string键和value编码`；array=`06 + u32元素数 + 各value编码`。不支持float，长度超u32或资源预算直接拒绝。最终取SHA-256；这是一套确定性编码规则，哈希使用标准库，不自造加密算法。序号字符串仍按其schema作为字符串编码，不能Qt一侧转数字、Go一侧保留字符串。

必须锁定的基础向量：null→`00`，false→`01`，空串→`0300000000`，整数1→`040000000000000001`，空数组→`0600000000`，空对象→`0500000000`；再由共享fixture给出复杂push的期望字节/摘要，不在测试中调用被测encoder生成期望。

已提供[canonical-v1基准向量](../../api/fixtures/canonical-v1.json)：15个独立手工字节向量及一个完整规范push，含semantic、expected_hex和expected_sha256；覆盖UTF-8与UTF-16排序不同的键及组合/分解Unicode。fixture是规范化后的语义树；validator/集合排序另由UT-HTTP-01/04/05验证，不能把仅通过encoder向量当作整个DecodePush通过。

## 6. 游标和分页令牌

CursorClaims={Version,KeyID,ServerInstance,OwnerID,DatasetID,DataEpoch,AfterSeq,ThroughSeq?,ExpiresAt}。对规范claims编码做HMAC-SHA256，token=`base64url(claimsBytes).base64url(mac)`；claims编码具体版本固定，验证时常量时间MAC比较，限制总长度和解码预算，查KeyID白名单。密钥按配置受保护，轮换保留仍在有效期的旧key；不把cursor当权限凭证。

tail cursor的ThroughSeq为空；请求在同一读快照固定当前head。分页未完时next cursor携带该ThroughSeq，防止无限追逐新写入。**has_more=false时返回after=through、ThroughSeq为空的tail cursor**，下一轮才能读取新的head；不能永久锁在旧截点。cursor有效期候选90天，日志底线检查仍独立；快照下载租期与cursor有效期不能混用。

签名无效/作用域不符不返回私密claims；epoch变化、key过期或after低于retained floor返回协议规定的恢复错误。after>through/head、负数、溢出和伪造key ID均拒绝。

## 7. 单元测试契约

| 用例ID | 输入/故障 | 必须断言 |
|---|---|---|
| UT-AUTH-01 | 大小写/空白登录名、错密码/不存在账号 | 正规化同源，统一拒绝；dummy受相同KDF限流 |
| UT-AUTH-02 | Hash期间停用/改密 | 写前复查失败，零session；未在writer内Hash |
| UT-AUTH-03 | 旧auth epoch、到期边界now==expiry | 拒绝，不自动续期 |
| UT-AUTH-04 | refresh成功 | current轮换，history保留，absolute expiry不延长，Commit后才发pair |
| UT-AUTH-05 | retired refresh重放 | Revoke已Commit后403；回调不是返回错误导致回滚 |
| UT-AUTH-06 | Revoke属于他人的session | 目标不变；不泄漏账号/设备信息 |
| UT-AUTH-07 | token源失败/摘要冲突/存储失败 | 有限尝试，无旧session覆盖，无凭据日志 |
| UT-AUTH-08 | ChangePassword和并发Login | credential_version CAS，旧session失效，不能混用两版哈希 |
| UT-HTTP-01 | 重复键/尾JSON/未知字段/非法UTF-8/溢出 | 400/422，应用用例未调用 |
| UT-HTTP-02 | body/解压预算超限/认证无效 | 413/401，无writer调用，日志无body/token |
| UT-HTTP-03 | Fault各类别 | status/code/retryable/Retry-After准确，无SQL泄漏 |
| UT-HTTP-04 | null/缺失、Unicode不同正规形 | 按定义区分，未知变换不发生 |
| UT-HTTP-05 | 对象键/preconditions顺序变化、command顺序变化 | 前者同摘要，后者不同；符合独立期望向量 |
| UT-HTTP-06 | tail→固定head多页→tail | 固定期间不追新head；完成后下一轮可见新写 |
| UT-HTTP-07 | cursor篡改/跨资料/过期/旧key | 不调用业务读取或按明确410恢复，不能跳序 |
| CT-AUTH-01 | 真实Argon2id标准向量、生产PHC往返、损坏/超预算参数 | 与独立标准期望一致，格式/常量比较/参数上限生效，不用FakeHasher证明 |
| CT-HTTP-01 | 实际TLS httptest连接、错误证书、慢/超额压缩body | 正确信任才成功，错误不降级，期限/解码预算可观察且无泄密 |

再以真实Store/TLS服务器完成INT-03～05和INT-09，不能用FakeHasher/Recorder宣称真实认证与传输通过。
