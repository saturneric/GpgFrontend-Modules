<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>GTrC</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="88"/>
        <source>The following email addresses have status:
</source>
        <translation>以下电子邮件地址的状态为：</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="98"/>
        <source>Could not parse status information.</source>
        <translation>无法解析状态信息。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="105"/>
        <source>Public Key Upload Successful</source>
        <translation>公钥上传成功</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="106"/>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation>公钥已成功上传至密钥服务器 keys.openpgp.org。
指纹：%1

%2
请检查您的电子邮件（%3）以获取 keys.openpgp.org 的进一步验证信息。

注意：如需了解验证详情，请访问：https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="124"/>
        <source>Key Upload Failed</source>
        <translation>密钥上传失败</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="125"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>无法将公钥上传至服务器。
指纹：%1
错误：%2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="157"/>
        <source>Key Update Failed</source>
        <translation>密钥更新失败</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="158"/>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation>无法从服务器获取公钥。
密钥 ID：%1
错误：%2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="210"/>
        <source>Key Server</source>
        <translation>密钥服务器</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="211"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>从受信任的密钥服务器导入公钥。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="259"/>
        <source>Key Server Operations</source>
        <translation>密钥服务器操作</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="264"/>
        <source>Publish Public Key to Key Server</source>
        <translation>将公钥发布到密钥服务器</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="272"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>从密钥服务器更新公钥</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="66"/>
        <source>Key ID</source>
        <translation>密钥 ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="66"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="66"/>
        <source>Creation Date</source>
        <translation>创建日期</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="67"/>
        <source>Expiration Date</source>
        <translation>到期日期</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="67"/>
        <source>Algorithm</source>
        <translation>算法</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="67"/>
        <source>Key Size</source>
        <translation>密钥大小</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="68"/>
        <source>Status</source>
        <translation>状态</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="71"/>
        <source>By Key ID</source>
        <translation>按密钥 ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="72"/>
        <source>By Email</source>
        <translation>按电子邮件</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="73"/>
        <source>By Fingerprint</source>
        <translation>按指纹</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="108"/>
        <source>Search value is empty.</source>
        <translation>搜索值不能为空。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="124"/>
        <source>Key server URL is empty.</source>
        <translation>密钥服务器 URL 不能为空。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="133"/>
        <source>Invalid key server URL format.</source>
        <translation>密钥服务器 URL 格式无效。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="146"/>
        <source>Invalid email format.</source>
        <translation>电子邮件格式无效。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="159"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation>指纹格式无效。应为长度为 16 或 40 的十六进制字符串。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="173"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>密钥 ID 格式无效。应为长度为 8 或 16 的十六进制字符串。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Unknown search type.</source>
        <translation>未知的搜索类型。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="289"/>
        <source>No GPG context is available.</source>
        <translation>没有可用的 GPG 上下文。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="14"/>
        <source>Search Keys</source>
        <translation>搜索密钥</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="36"/>
        <source>Key Server</source>
        <translation>密钥服务器</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="56"/>
        <source>Search Type</source>
        <translation>搜索类型</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="66"/>
        <source>Search Value</source>
        <translation>搜索值</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="85"/>
        <source>Search</source>
        <translation>搜索</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="105"/>
        <source>Tips: double click to import the selected key.</source>
        <translation>提示：双击可导入所选密钥。</translation>
    </message>
</context>
</TS>
