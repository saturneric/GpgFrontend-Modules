<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_TW">
<context>
    <name>GTrC</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="88"/>
        <source>The following email addresses have status:
</source>
        <translation>以下電子郵件地址的狀態為：</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="98"/>
        <source>Could not parse status information.</source>
        <translation>無法解析狀態資訊。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="105"/>
        <source>Public Key Upload Successful</source>
        <translation>公鑰上傳成功</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="106"/>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation>公鑰已成功上傳至金鑰伺服器 keys.openpgp.org。
指紋：%1

%2
請檢查您的電子郵件（%3）以接收 keys.openpgp.org 的進一步驗證通知。

注意：如需驗證相關資訊，請參閱此處：https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="124"/>
        <source>Key Upload Failed</source>
        <translation>金鑰上傳失敗</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="125"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>無法將公鑰上傳至伺服器。
指紋：%1
錯誤：%2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="157"/>
        <source>Key Update Failed</source>
        <translation>金鑰更新失敗</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="158"/>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation>無法從伺服器擷取公鑰。
金鑰 ID：%1
錯誤：%2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="210"/>
        <source>Key Server</source>
        <translation>金鑰伺服器</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="211"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>從受信任的金鑰伺服器匯入公鑰。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="259"/>
        <source>Key Server Operations</source>
        <translation>金鑰伺服器操作</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="264"/>
        <source>Publish Public Key to Key Server</source>
        <translation>將公鑰發佈至金鑰伺服器</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="272"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>從金鑰伺服器更新公鑰</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="66"/>
        <source>Key ID</source>
        <translation>金鑰 ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="66"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="66"/>
        <source>Creation Date</source>
        <translation>建立日期</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="67"/>
        <source>Expiration Date</source>
        <translation>到期日期</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="67"/>
        <source>Algorithm</source>
        <translation>演算法</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="67"/>
        <source>Key Size</source>
        <translation>金鑰大小</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="68"/>
        <source>Status</source>
        <translation>狀態</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="71"/>
        <source>By Key ID</source>
        <translation>依金鑰 ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="72"/>
        <source>By Email</source>
        <translation>依電子郵件</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="73"/>
        <source>By Fingerprint</source>
        <translation>依指紋</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="108"/>
        <source>Search value is empty.</source>
        <translation>搜尋值為空。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="124"/>
        <source>Key server URL is empty.</source>
        <translation>金鑰伺服器 URL 為空。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="133"/>
        <source>Invalid key server URL format.</source>
        <translation>金鑰伺服器 URL 格式無效。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="146"/>
        <source>Invalid email format.</source>
        <translation>電子郵件格式無效。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="159"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation>指紋格式無效。應為長度 16 或 40 的十六進位字串。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="173"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>金鑰 ID 格式無效。應為長度 8 或 16 的十六進位字串。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Unknown search type.</source>
        <translation>未知的搜尋類型。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="289"/>
        <source>No GPG context is available.</source>
        <translation>沒有可用的 GPG 環境。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="14"/>
        <source>Search Keys</source>
        <translation>搜尋金鑰</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="36"/>
        <source>Key Server</source>
        <translation>金鑰伺服器</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="56"/>
        <source>Search Type</source>
        <translation>搜尋類型</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="66"/>
        <source>Search Value</source>
        <translation>搜尋值</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="85"/>
        <source>Search</source>
        <translation>搜尋</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="105"/>
        <source>Tips: double click to import the selected key.</source>
        <translation>提示：雙擊可匯入所選金鑰。</translation>
    </message>
</context>
</TS>
