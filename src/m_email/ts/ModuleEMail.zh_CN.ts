<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>EMailMetaDataDialog</name>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="20"/>
        <source>Message</source>
        <translation>邮件消息</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="54"/>
        <source>From</source>
        <translation>发件人</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="77"/>
        <source>To</source>
        <translation>收件人</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="100"/>
        <location filename="../EMailMetaDataDialog.ui" line="207"/>
        <source>CC</source>
        <translation>抄送</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="123"/>
        <location filename="../EMailMetaDataDialog.ui" line="214"/>
        <source>BCC</source>
        <translation>密送</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="153"/>
        <source>Subject</source>
        <translation>主题</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="175"/>
        <source>Tips: You can fill in multiple email addresses, please separate them with &quot;;&quot;, except for the &apos;From&apos; field.</source>
        <translation>提示： 您可以填写多个电子邮件地址，请用&quot;;&quot;分隔，但&apos;发件人&apos;字段除外。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="234"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="241"/>
        <source>OK</source>
        <translation>确认</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="196"/>
        <source>The &apos;From&apos; field cannot be empty.</source>
        <translation>&apos;发件人&apos;字段不能为空。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="198"/>
        <source>The &apos;From&apos; field must contain a valid email address.</source>
        <translation>&apos;发件人&apos;字段必须包含有效的电子邮件地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="202"/>
        <source>The &apos;To&apos; field cannot be empty.</source>
        <translation>收件人&apos;字段不能为空。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="206"/>
        <source>One or more &apos;To&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>一个或多个&apos;收件人&apos;地址无效。请用&quot;;&quot;分隔多个地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="213"/>
        <source>One or more &apos;CC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>一个或多个 &apos;抄送&apos; 地址无效。请用&quot;;&quot;分隔多个地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="221"/>
        <source>One or more &apos;BCC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>一个或多个 &apos;密送&apos; 地址无效。请用&quot;;&quot;分隔多个地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="227"/>
        <source>The &apos;Subject&apos; field cannot be empty.</source>
        <translation>主题&apos;字段不能为空。</translation>
    </message>
</context>
<context>
    <name>EMailModule</name>
    <message>
        <location filename="../EMailModule.cpp" line="219"/>
        <source># EML Data Error

The provided EML data does not conform to RFC 3156 standards and cannot be processed.

**Details:** %1

### What is EML Data?
EML is a file format for representing email messages, typically including headers, body text, attachments, and metadata. Complete and properly structured EML data is required for validation.

### Suggested Solutions
1. Verify the EML data is complete and matches the structure outlined in RFC 3156.
2. Refer to the official documentation for the EML structure: %2

After correcting the EML data, try the operation again.</source>
        <translation># EML 数据错误

提供的 EML 数据不符合 RFC 3156 标准，无法处理。

**详细信息：** %1

### 什么是 EML 数据？
EML 是一种用于表示电子邮件的文件格式，通常包含头部、正文、附件和元数据。验证需要完整且结构正确的 EML 数据。

### 建议解决方案
1. 验证 EML 数据是否完整，并符合 RFC 3156 规定的结构。
2. 查阅 EML 结构的官方文档： %2

修正 EML 数据后，请重试该操作。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="244"/>
        <source># Email Operation Error

An error occurred during the email operation. The process could not be completed.

**Details:**
- **Error Code:** %1
- **Error Message:** %2

### Possible Causes
1. The email data may be incomplete or corrupted.
2. The selected GPG key does not have the necessary permissions.
3. Issues in the GPG environment or configuration.

### Suggested Solutions
1. Ensure the email data is complete and follows the expected format.
2. Verify the GPG key has the required access permissions.
3. Check your GPG environment and configuration settings.
4. Review the error details above or application logs for further troubleshooting.

If the issue persists, consider seeking technical support or consulting the documentation.</source>
        <translation># 电子邮件操作错误

电子邮件操作过程中发生错误，无法完成处理。

**详细信息：**
- **错误代码：** %1
- **错误信息：** %2

### 可能原因
1. 电子邮件数据可能不完整或已损坏。
2. 所选 GPG 密钥缺乏必要的权限。
3. GPG 环境或配置存在问题。

### 建议解决方案
1. 确保电子邮件数据完整且符合预期格式。
2. 验证 GPG 密钥是否具有所需的访问权限。
3. 检查您的 GPG 环境和配置设置。
4. 查看上述错误详细信息或应用程序日志以进行进一步排查。

如果问题仍然存在，请寻求技术支持或查阅相关文档。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="93"/>
        <location filename="../EMailModule.cpp" line="415"/>
        <location filename="../EMailModule.cpp" line="546"/>
        <location filename="../EMailModule.cpp" line="1230"/>
        <source>From</source>
        <translation>发件人</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="92"/>
        <source>E-Mail</source>
        <translation>电子邮件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="94"/>
        <location filename="../EMailModule.cpp" line="418"/>
        <location filename="../EMailModule.cpp" line="549"/>
        <location filename="../EMailModule.cpp" line="1233"/>
        <source>To</source>
        <translation>收件人</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="95"/>
        <location filename="../EMailModule.cpp" line="422"/>
        <location filename="../EMailModule.cpp" line="553"/>
        <location filename="../EMailModule.cpp" line="1237"/>
        <source>Subject</source>
        <translation>主题</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="96"/>
        <location filename="../EMailModule.cpp" line="425"/>
        <location filename="../EMailModule.cpp" line="556"/>
        <location filename="../EMailModule.cpp" line="1240"/>
        <source>CC</source>
        <translation>抄送</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="97"/>
        <location filename="../EMailModule.cpp" line="428"/>
        <location filename="../EMailModule.cpp" line="559"/>
        <location filename="../EMailModule.cpp" line="1243"/>
        <source>BCC</source>
        <translation>密送</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="98"/>
        <location filename="../EMailModule.cpp" line="431"/>
        <location filename="../EMailModule.cpp" line="562"/>
        <location filename="../EMailModule.cpp" line="1246"/>
        <source>Date</source>
        <translation>日期</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="105"/>
        <source>OpenPGP</source>
        <translation>OpenPGP</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="106"/>
        <location filename="../EMailModule.cpp" line="439"/>
        <location filename="../EMailModule.cpp" line="1254"/>
        <source>Signed EML Data Hash (SHA1)</source>
        <translation>已签名 EML 数据哈希值 (SHA1)</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="108"/>
        <location filename="../EMailModule.cpp" line="444"/>
        <location filename="../EMailModule.cpp" line="1259"/>
        <source>Message Integrity Check Algorithm</source>
        <translation>消息完整性检查算法</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="136"/>
        <source>Encryption Recipient</source>
        <translation>加密接收者</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="137"/>
        <source>Recipient</source>
        <translation>接收者</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="138"/>
        <source>Key ID</source>
        <translation>密钥 ID</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="453"/>
        <source>Verify E-Mail</source>
        <translation>验证电子邮件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="571"/>
        <source>Decrypt E-Mail</source>
        <translation>解密电子邮件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="748"/>
        <location filename="../EMailModule.cpp" line="773"/>
        <source>Sign E-Mail</source>
        <translation>签名电子邮件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="933"/>
        <location filename="../EMailModule.cpp" line="970"/>
        <source>Encrypt E-Mail</source>
        <translation>加密电子邮件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1103"/>
        <location filename="../EMailModule.cpp" line="1150"/>
        <source>Encrypt and Sign E-Mail</source>
        <translation>加密并签名电子邮件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1268"/>
        <source>Decrypt and Verify E-Mail</source>
        <translation>解密并验证电子邮件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1316"/>
        <source>Save file</source>
        <translation>保存文件</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1343"/>
        <location filename="../EMailModule.cpp" line="1388"/>
        <location filename="../EMailModule.cpp" line="1415"/>
        <source>Warning</source>
        <translation>警告</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1344"/>
        <source>Cannot read file%1:
%2.</source>
        <translation>无法读取文件%1：
%2。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1389"/>
        <source>The file %1 is too large (%2 bytes) to be opened. The maximum allowed size is 1 MB.</source>
        <translation>文件 %1 太大（%2 字节），无法打开。允许的最大大小为 1 MB。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1416"/>
        <source>Cannot read file %1:
%2.</source>
        <translation>无法读取文件 %1：
%2。</translation>
    </message>
</context>
<context>
    <name>GTrC</name>
    <message>
        <location filename="../EMailModule.cpp" line="320"/>
        <source>Mail Editor</source>
        <translation>邮件编辑器</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="322"/>
        <source>Open a new text editor for email.</source>
        <translation>打开新的电子邮件文本编辑器。</translation>
    </message>
</context>
</TS>
