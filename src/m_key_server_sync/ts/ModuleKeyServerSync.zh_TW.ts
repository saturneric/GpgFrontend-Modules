<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_TW">
<context>
    <name>GTrC</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="252"/>
        <source>The following email addresses have status:
</source>
        <translation>以下電子郵件地址的狀態為：</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="262"/>
        <source>Could not parse status information.</source>
        <translation>無法解析狀態資訊。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="230"/>
        <location filename="../KeyServerSyncModule.cpp" line="273"/>
        <source>Public Key Upload Successful</source>
        <translation>公鑰上傳成功</translation>
    </message>
    <message>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation type="vanished">公鑰已成功上傳至金鑰伺服器 keys.openpgp.org。
指紋：%1

%2
請檢查您的電子郵件（%3）以接收 keys.openpgp.org 的進一步驗證通知。

注意：如需驗證相關資訊，請參閱此處：https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="189"/>
        <location filename="../KeyServerSyncModule.cpp" line="217"/>
        <location filename="../KeyServerSyncModule.cpp" line="289"/>
        <source>Key Upload Failed</source>
        <translation>金鑰上傳失敗</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="190"/>
        <source>Failed to export the public key before uploading.
Key: %1</source>
        <translation>上傳前匯出公鑰失敗。
金鑰：%1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="274"/>
        <source>The public key was successfully uploaded to the key server %4.
Fingerprint: %1

%2
Please check your email (%3) for further verification from %4.</source>
        <translation>公鑰已成功上傳至金鑰伺服器 %4。
指紋：%1

%2
請檢查您的電子郵件（%3）以接收 %4 的進一步驗證通知。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="218"/>
        <location filename="../KeyServerSyncModule.cpp" line="290"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>無法將公鑰上傳至伺服器。
指紋：%1
錯誤：%2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="101"/>
        <source>The key server did not return a key.</source>
        <translation>金鑰伺服器未回傳金鑰。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="127"/>
        <source>Publish Without Verification?</source>
        <translation>不經驗證即發布？</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="129"/>
        <source>%1 does not support verified publishing (VKS), so the key would be uploaded over HKP instead.

The server will not confirm your email address, and the upload cannot be undone — HKP key servers do not let keys be removed.

Publish to %1 anyway?</source>
        <translation>%1 不支援驗證發布（VKS），因此金鑰將改為透過 HKP 上傳。

伺服器不會確認您的電子郵件地址，且上傳無法撤銷——HKP 金鑰伺服器不允許移除金鑰。

仍要發布至 %1 嗎？</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="232"/>
        <source>The public key was uploaded to the key server %2 over HKP.
Fingerprint: %1</source>
        <translation>公鑰已透過 HKP 上傳至金鑰伺服器 %2。
指紋：%1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="325"/>
        <source>Key Update Failed</source>
        <translation>金鑰更新失敗</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="326"/>
        <source>Failed to retrieve public key from %3.
Key ID: %1
Error: %2</source>
        <translation>無法從 %3 擷取公鑰。
金鑰 ID：%1
錯誤：%2</translation>
    </message>
    <message>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation type="vanished">無法從伺服器擷取公鑰。
金鑰 ID：%1
錯誤：%2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="375"/>
        <source>Key Server</source>
        <translation>金鑰伺服器</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="376"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>從受信任的金鑰伺服器匯入公鑰。</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="424"/>
        <source>Key Server Operations</source>
        <translation>金鑰伺服器操作</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="429"/>
        <source>Publish Public Key to Key Server</source>
        <translation>將公鑰發佈至金鑰伺服器</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="436"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>從金鑰伺服器更新公鑰</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="247"/>
        <source>The server could not be reached.</source>
        <translation>無法連線至伺服器。</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="249"/>
        <source>The server responded, but not as a key server: it supports neither the HKP nor the VKS interface.</source>
        <translation>伺服器已回應，但並非金鑰伺服器：既不支援 HKP 也不支援 VKS 介面。</translation>
    </message>
</context>
<context>
    <name>KeyServerSettingsPage</name>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="17"/>
        <location filename="../KeyServerSettingsPage.cpp" line="58"/>
        <source>Key Server List</source>
        <translation>金鑰伺服器清單</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="49"/>
        <location filename="../KeyServerSettingsPage.cpp" line="59"/>
        <source>Add a Key Server</source>
        <translation>新增金鑰伺服器</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="82"/>
        <location filename="../KeyServerSettingsPage.cpp" line="60"/>
        <source>Operations</source>
        <translation>操作</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="60"/>
        <location filename="../KeyServerSettingsPage.cpp" line="62"/>
        <source>Add</source>
        <translation>新增</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="88"/>
        <location filename="../KeyServerSettingsPage.cpp" line="63"/>
        <source>Set As Default</source>
        <translation>設為預設</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="95"/>
        <location filename="../KeyServerSettingsPage.cpp" line="64"/>
        <source>Test Selected</source>
        <translation>測試選取項目</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="102"/>
        <location filename="../KeyServerSettingsPage.cpp" line="65"/>
        <source>Delete Selected</source>
        <translation>刪除選取項目</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="67"/>
        <source>https://keys.example.org</source>
        <translation>https://keys.example.org</translation>
    </message>
    <message>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP; publishing and refreshing use VKS.</source>
        <translation type="vanished">新增金鑰伺服器前，會先測試其 HKP 與 VKS 介面。搜尋使用 HKP；發布與重新整理使用 VKS。</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="68"/>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP. Publishing and refreshing always use the default server: over VKS where it offers it, over HKP otherwise.</source>
        <translation>新增金鑰伺服器前，會先測試其 HKP 與 VKS 介面。搜尋使用 HKP。發布與重新整理一律使用預設伺服器：若支援 VKS 則使用 VKS，否則使用 HKP。</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Default</source>
        <translation>預設</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Address</source>
        <translation>位址</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>HKP</source>
        <translation>HKP</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>VKS</source>
        <translation>VKS</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Status</source>
        <translation>狀態</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="76"/>
        <source>Last Tested</source>
        <translation>上次測試</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="108"/>
        <source>yes</source>
        <translation>是</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="109"/>
        <source>no</source>
        <translation>否</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Verified</source>
        <translation>已驗證</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Unverified</source>
        <translation>未驗證</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="136"/>
        <source>never</source>
        <translation>從未</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="162"/>
        <source>Invalid Address</source>
        <translation>無效位址</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="163"/>
        <source>&quot;%1&quot; is not a valid key server address.</source>
        <translation>「%1」不是有效的金鑰伺服器位址。</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="173"/>
        <source>Already Listed</source>
        <translation>已列出</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="174"/>
        <source>%1 is already in the key server list.</source>
        <translation>%1 已存在於金鑰伺服器列表中。</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="180"/>
        <source>Insecure Key Server Address</source>
        <translation>不安全的金鑰伺服器位址</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="181"/>
        <source>%1 uses plain HTTP, so anyone on the network can see and change what you look up or publish. Add it anyway?</source>
        <translation>%1 使用純 HTTP，因此網路上的任何人都可以查看或變更您查詢或發布的內容。仍要新增嗎？</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="229"/>
        <source>No Verified Publishing</source>
        <translation>無驗證發布</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="230"/>
        <source>%1 does not support the VKS interface, so publishing and refreshing will use HKP instead.

Over HKP the server does not confirm your email address, and an uploaded key cannot be removed again.

Use %1 as the default anyway?</source>
        <translation>%1 不支援 VKS 介面，因此發布與重新整理將改用 HKP。

透過 HKP，伺服器不會確認您的電子郵件地址，且上傳的金鑰無法再次移除。

是否仍要將 %1 設為預設？</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="291"/>
        <source>Key Server Not Verified</source>
        <translation>金鑰伺服器未驗證</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="292"/>
        <source>%1 did not answer as a key server.

%2

It has been added and marked unverified; use Test Selected to try again.</source>
        <translation>%1 未以金鑰伺服器身分回應。

%2

它已新增並標記為未驗證；請使用「測試選取項目」再試一次。</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Key ID</source>
        <translation>金鑰 ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Creation Date</source>
        <translation>建立日期</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Expiration Date</source>
        <translation>到期日期</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Algorithm</source>
        <translation>演算法</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Key Size</source>
        <translation>金鑰大小</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="112"/>
        <source>Status</source>
        <translation>狀態</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="115"/>
        <source>By Key ID</source>
        <translation>依金鑰 ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="116"/>
        <source>By Email</source>
        <translation>依電子郵件</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="117"/>
        <source>By Fingerprint</source>
        <translation>依指紋</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="120"/>
        <source>Enter a value, then press Enter or Search</source>
        <translation>輸入數值後，按 Enter 或搜尋</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="163"/>
        <source>Search value is empty.</source>
        <translation>搜尋值為空。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Key server URL is empty.</source>
        <translation>金鑰伺服器 URL 為空。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="188"/>
        <source>Invalid key server URL format.</source>
        <translation>金鑰伺服器 URL 格式無效。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="201"/>
        <source>Invalid email format.</source>
        <translation>電子郵件格式無效。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="216"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16, 40 or 64.</source>
        <translation>指紋格式無效。應為長度 16、40 或 64 的十六進位字串。</translation>
    </message>
    <message>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation type="vanished">指紋格式無效。應為長度 16 或 40 的十六進位字串。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="230"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>金鑰 ID 格式無效。應為長度 8 或 16 的十六進位字串。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="236"/>
        <source>Unknown search type.</source>
        <translation>未知的搜尋類型。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="273"/>
        <location filename="../SearchKeyDialog.cpp" line="283"/>
        <source>No keys found matching your search.</source>
        <translation>找不到符合搜尋條件的金鑰。</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="302"/>
        <source>(no user ID published)</source>
        <translation>（未發布使用者 ID）</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="306"/>
        <source>Unknown</source>
        <translation>未知</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="310"/>
        <source>Never</source>
        <translation>從未</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="374"/>
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
