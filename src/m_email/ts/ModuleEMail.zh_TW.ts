<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_TW">
<context>
    <name>EMailMetaDataDialog</name>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="196"/>
        <source>The &apos;From&apos; field cannot be empty.</source>
        <translation>「寄件者」欄位不能為空。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="198"/>
        <source>The &apos;From&apos; field must contain a valid email address.</source>
        <translation>「寄件者」欄位必須包含有效的電子郵件地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="202"/>
        <source>The &apos;To&apos; field cannot be empty.</source>
        <translation>「收件者」欄位不能為空。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="206"/>
        <source>One or more &apos;To&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>一個或多個「收件者」地址無效。請使用「;」分隔多個地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="213"/>
        <source>One or more &apos;CC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>一個或多個「抄送」地址無效。請使用「;」分隔多個地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="221"/>
        <source>One or more &apos;BCC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>一個或多個「密件副本」地址無效。請使用「;」分隔多個地址。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="227"/>
        <source>The &apos;Subject&apos; field cannot be empty.</source>
        <translation>「主旨」欄位不能為空。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="20"/>
        <source>Message</source>
        <translation>訊息</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="54"/>
        <source>From</source>
        <translation>寄件者</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="77"/>
        <source>To</source>
        <translation>收件者</translation>
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
        <translation>密件副本</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="153"/>
        <source>Subject</source>
        <translation>主旨</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="175"/>
        <source>Tips: You can fill in multiple email addresses, please separate them with &quot;;&quot;, except for the &apos;From&apos; field.</source>
        <translation>提示：除「寄件者」欄位外，您可以填寫多個電子郵件地址，請以「;」分隔。</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="234"/>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="241"/>
        <source>OK</source>
        <translation>確定</translation>
    </message>
</context>
<context>
    <name>EMailModule</name>
    <message>
        <location filename="../EMailModule.cpp" line="109"/>
        <source># EML Data Error

The provided EML data does not conform to RFC 3156 standards and cannot be processed.

**Details:** %1

### What is EML Data?
EML is a file format for representing email messages, typically including headers, body text, attachments, and metadata. Complete and properly structured EML data is required for validation.

### Suggested Solutions
1. Verify the EML data is complete and matches the structure outlined in RFC 3156.
2. Refer to the official documentation for the EML structure: %2

After correcting the EML data, try the operation again.</source>
        <translation># EML 資料錯誤

提供的 EML 資料不符合 RFC 3156 標準，無法處理。

**詳細資訊：** %1

### 什麼是 EML 資料？
EML 是一種用於表示電子郵件訊息的檔案格式，通常包含標頭、內文、附件和中繼資料。驗證需要完整且結構正確的 EML 資料。

### 建議解決方案
1. 確認 EML 資料完整且符合 RFC 3156 所述的結構。
2. 參閱 EML 結構的官方文件：%2

修正 EML 資料後，請再次嘗試操作。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="134"/>
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
        <translation># 電子郵件操作錯誤

電子郵件操作期間發生錯誤，無法完成處理。

**詳細資訊：**
- **錯誤代碼：** %1
- **錯誤訊息：** %2

### 可能原因
1. 電子郵件資料可能不完整或已損毀。
2. 選取的 GPG 金鑰缺乏必要的權限。
3. GPG 環境或設定存在問題。

### 建議解決方案
1. 確保電子郵件資料完整且符合預期格式。
2. 驗證 GPG 金鑰是否具有所需的存取權限。
3. 檢查您的 GPG 環境與設定。
4. 檢視上述錯誤詳細資訊或應用程式記錄檔，以進行進一步疑難排解。

若問題持續發生，請尋求技術支援或查閱文件。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="301"/>
        <location filename="../EMailModule.cpp" line="421"/>
        <location filename="../EMailModule.cpp" line="1041"/>
        <source>From</source>
        <translation>寄件者</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="304"/>
        <location filename="../EMailModule.cpp" line="424"/>
        <location filename="../EMailModule.cpp" line="1044"/>
        <source>To</source>
        <translation>收件者</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="308"/>
        <location filename="../EMailModule.cpp" line="428"/>
        <location filename="../EMailModule.cpp" line="1048"/>
        <source>Subject</source>
        <translation>主旨</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="311"/>
        <location filename="../EMailModule.cpp" line="431"/>
        <location filename="../EMailModule.cpp" line="1051"/>
        <source>CC</source>
        <translation>副本</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="314"/>
        <location filename="../EMailModule.cpp" line="434"/>
        <location filename="../EMailModule.cpp" line="1054"/>
        <source>BCC</source>
        <translation>密件副本</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="317"/>
        <location filename="../EMailModule.cpp" line="437"/>
        <location filename="../EMailModule.cpp" line="1057"/>
        <source>Date</source>
        <translation>日期</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="325"/>
        <location filename="../EMailModule.cpp" line="1065"/>
        <source>Signed EML Data Hash (SHA1)</source>
        <translation>已簽署 EML 資料雜湊值 (SHA1)</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="330"/>
        <location filename="../EMailModule.cpp" line="1070"/>
        <source>Message Integrity Check Algorithm</source>
        <translation>訊息完整性檢查演算法</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1120"/>
        <source>Save file</source>
        <translation>儲存檔案</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1147"/>
        <location filename="../EMailModule.cpp" line="1192"/>
        <location filename="../EMailModule.cpp" line="1219"/>
        <source>Warning</source>
        <translation>警告</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1148"/>
        <source>Cannot read file%1:
%2.</source>
        <translation>無法讀取檔案%1：
%2。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1193"/>
        <source>The file %1 is too large (%2 bytes) to be opened. The maximum allowed size is 1 MB.</source>
        <translation>檔案 %1 太大（%2 位元組），無法開啟。允許的最大容量為 1 MB。</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1220"/>
        <source>Cannot read file %1:
%2.</source>
        <translation>無法讀取檔案 %1：
%2。</translation>
    </message>
</context>
<context>
    <name>GTrC</name>
    <message>
        <location filename="../EMailModule.cpp" line="210"/>
        <source>Mail Editor</source>
        <translation>電子郵件編輯器</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="212"/>
        <source>Open a new text editor for email.</source>
        <translation>開啟新的電子郵件文字編輯器。</translation>
    </message>
</context>
</TS>
