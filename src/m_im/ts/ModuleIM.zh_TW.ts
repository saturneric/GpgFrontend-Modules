<?xml version='1.0' encoding='utf-8'?>
<TS version="2.1">
    <context>
        <name>ModuleIM</name>
        <message>
            <location filename="../ImCodec.cpp" line="58" />
            <source>Encoding</source>
            <translation>編碼</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="59" />
            <source>Container Format</source>
            <translation>容器格式</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="61" />
            <source>Message Book</source>
            <translation>訊息簿</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="62" />
            <source>Shared phrase (Argon2id)</source>
            <translation>共享片語 (Argon2id)</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="63" />
            <source>Default, no shared phrase set</source>
            <translation>預設，未設定共用短語</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="67" />
            <location filename="../ImSettingsPage.cpp" line="158" />
            <source>Book Fingerprint</source>
            <translation>書本指紋</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Set a Phrase</source>
            <translation>設定短語</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Settings &gt; Instant Messaging</source>
            <translation>設定 &gt; 即時通訊</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>OpenPGP Payload</source>
            <translation>OpenPGP 負載</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>%1 bytes</source>
            <translation>%1 位元組</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>Token Length</source>
            <translation>令牌長度</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>%1 characters</source>
            <translation>%1 個字元</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="82" />
            <source>Wire Overhead</source>
            <translation>線路開銷</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="87" />
            <location filename="../ImModule.cpp" line="249" />
            <source>Instant Messaging</source>
            <translation>即時通訊</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="96" />
            <source>An Instant Messaging section followed by the OpenPGP result.</source>
            <translation>即時訊息區段後接 OpenPGP 結果。</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="126" />
            <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
            <translation>此訊息過長，無法以即時訊息傳送。

加密後的訊息為 %1 位元組，而即時訊息格式最多承載 %2。請縮短文字，或改以一般 OpenPGP 訊息傳送。</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="138" />
            <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
            <translation>無法準備即時訊息：加密後的訊息無法轉換為權杖。</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="85" />
            <location filename="../ImModule.cpp" line="98" />
            <source>Instant Message Token</source>
            <translation>即時訊息權杖</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="86" />
            <source>Recognise and unwrap instant messaging tokens before decrypting</source>
            <translation>解密前識別並解開即時通訊權杖</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="99" />
            <source>Wrap an encrypted message as an instant messaging token</source>
            <translation>將加密訊息封裝為即時通訊權杖</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="134" />
            <source>No Message Book Phrase Set</source>
            <translation>未設定訊息書短語</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="135" />
            <source>You have not set a Message Book phrase.</source>
            <translation>您尚未設定訊息書短語。</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="136" />
            <source>Instant messages are hidden using a shared "Message Book". Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
            <translation>即時訊息是透過共享的「訊息書」來隱藏的。若無短語，GpgFrontend 會退回使用內建的預設書，而該書包含在程式的每個副本中。它可對簡單的掃描器隱藏格式，但任何了解 GpgFrontend 的人仍可辨識您的訊息的本質。

無論如何，您的訊息都是 OpenPGP 加密的；此處的關鍵僅在於它是否可被識別為加密訊息。

為達到此目的，請設定一個短語，並私下與您通訊的對象分享。你們雙方必須使用完全相同的短語。</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="148" />
            <source>Open Settings...</source>
            <translation>開啟設定...</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="150" />
            <source>Continue with Default</source>
            <translation>使用預設值繼續</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="151" />
            <source>Continue, Don't Ask Again</source>
            <translation>繼續，不再詢問</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="181" />
            <source>IM Encrypt</source>
            <translation>IM 加密</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="182" />
            <source>Encrypt the message as a single-line token for chat apps</source>
            <translation>將訊息加密為適用於聊天應用程式的單行權杖</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="194" />
            <source>IM Encrypt &amp;&amp; Sign</source>
            <translation>IM 加密 &amp;&amp; 簽署</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="195" />
            <source>Encrypt and sign the message as a single-line token for chat apps</source>
            <translation>將訊息加密並簽署為適用於聊天應用程式的單行權杖</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="250" />
            <source>instant messaging,message book,phrase,fingerprint,token</source>
            <translation>instant messaging,message book,phrase,fingerprint,token,即時通訊,訊息簿,短語,指紋,權杖</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="73" />
            <source>Message Book Phrase</source>
            <translation>訊息簿密語</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="77" />
            <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
            <translation>一個您與朋友共享的長秘密。它讓您的訊息看起來像隨機文字，因此沒人能夠看出它們是 PGP。您和朋友必須使用完全相同的短語。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="92" />
            <source>No phrase set. Messages use the built-in default book.</source>
            <translation>未設定密語。訊息將使用內建的預設簿。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="108" />
            <source>Generate</source>
            <translation>產生</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="109" />
            <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
            <translation>建立一個新的隨機密語。與朋友分享，以便你們使用相同的密語。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="120" />
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Show</source>
            <translation>顯示</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="121" />
            <source>Show or hide the phrase.</source>
            <translation>顯示或隱藏密語。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="125" />
            <location filename="../ImSettingsPage.cpp" line="177" />
            <source>Copy</source>
            <translation>複製</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="126" />
            <source>Copy the phrase to the clipboard.</source>
            <translation>將密語複製到剪貼簿。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="130" />
            <source>Paste</source>
            <translation>貼上</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="132" />
            <source>Replace the phrase with the one on the clipboard.</source>
            <translation>以剪貼簿上的內容取代密語。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="138" />
            <source>Clear</source>
            <translation>清除</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="140" />
            <source>Remove the phrase and fall back to the default book.</source>
            <translation>移除片語並回復為預設書本。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="162" />
            <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
            <translation>由您的片語產生的簡短代碼。與朋友大聲朗讀，以確認雙方使用相同代碼。與片語不同，此代碼可以公開說出。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="179" />
            <source>Copy the fingerprint to the clipboard.</source>
            <translation>複製指紋到剪貼簿。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="191" />
            <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
            <translation>片語儲存在加密的快取中，絕不會儲存在設定檔中。請透過私人管道傳送給您的朋友。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Hide</source>
            <translation>隱藏</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="249" />
            <source>No phrase set. Using the built-in default.</source>
            <translation>未設定片語。使用內建預設值。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="250" />
            <source>Phrase set. %1 characters.</source>
            <translation>已設定片語。%1 個字元。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="260" />
            <source>Calculating…</source>
            <translation>計算中…</translation>
        </message>
    </context>
</TS>