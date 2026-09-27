<?xml version='1.0' encoding='utf-8'?>
<TS version="2.1">
    <context>
        <name>ModuleIM</name>
        <message>
            <location filename="../ImCodec.cpp" line="58" />
            <source>Encoding</source>
            <translation>编码</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="59" />
            <source>Container Format</source>
            <translation>容器格式</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="61" />
            <source>Message Book</source>
            <translation>消息簿</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="62" />
            <source>Shared phrase (Argon2id)</source>
            <translation>共享口令（Argon2id）</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="63" />
            <source>Default, no shared phrase set</source>
            <translation>默认，未设置共享短语</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="67" />
            <location filename="../ImSettingsPage.cpp" line="158" />
            <source>Book Fingerprint</source>
            <translation>口令本指纹</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Set a Phrase</source>
            <translation>设置短语</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Settings &gt; Instant Messaging</source>
            <translation>设置 &gt; 即时通讯</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>OpenPGP Payload</source>
            <translation>OpenPGP 载荷</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>%1 bytes</source>
            <translation>%1 字节</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>Token Length</source>
            <translation>令牌长度</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>%1 characters</source>
            <translation>%1 字符</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="82" />
            <source>Wire Overhead</source>
            <translation>线路开销</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="87" />
            <location filename="../ImModule.cpp" line="249" />
            <source>Instant Messaging</source>
            <translation>即时通讯</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="96" />
            <source>An Instant Messaging section followed by the OpenPGP result.</source>
            <translation>即时消息部分后随 OpenPGP 结果。</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="126" />
            <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
            <translation>此消息过长，无法作为即时消息发送。

加密消息为 %1 字节，即时消息格式最多承载 %2。请缩短文本，或改为发送普通 OpenPGP 消息。</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="138" />
            <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
            <translation>准备即时消息失败：加密消息无法转换为令牌。</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="85" />
            <location filename="../ImModule.cpp" line="98" />
            <source>Instant Message Token</source>
            <translation>即时消息令牌</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="86" />
            <source>Recognise and unwrap instant messaging tokens before decrypting</source>
            <translation>解密前识别并解开即时通讯令牌</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="99" />
            <source>Wrap an encrypted message as an instant messaging token</source>
            <translation>将加密消息封装为即时通讯令牌</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="134" />
            <source>No Message Book Phrase Set</source>
            <translation>未设置消息簿短语</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="135" />
            <source>You have not set a Message Book phrase.</source>
            <translation>您尚未设置消息簿短语。</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="136" />
            <source>Instant messages are hidden using a shared "Message Book". Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
            <translation>即时消息通过共享的“消息簿”隐藏。如果没有设置短语，GpgFrontend 将回退到内置的默认消息簿，该消息簿随程序的每个副本提供。它可以对简单的扫描器隐藏格式，但任何了解 GpgFrontend 的人仍然可以识别出您的消息是什么。

无论如何，您的消息都是经过 OpenPGP 加密的；这里的关键仅在于它是否完全可被识别为加密消息。

要做到这一点，请设置一个短语，并与您通信的人私下分享。你们双方必须使用完全相同的短语。</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="148" />
            <source>Open Settings...</source>
            <translation>打开设置...</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="150" />
            <source>Continue with Default</source>
            <translation>使用默认值继续</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="151" />
            <source>Continue, Don't Ask Again</source>
            <translation>继续，不再询问</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="181" />
            <source>IM Encrypt</source>
            <translation>IM 加密</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="182" />
            <source>Encrypt the message as a single-line token for chat apps</source>
            <translation>将消息加密为适用于聊天应用的单行令牌</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="194" />
            <source>IM Encrypt &amp;&amp; Sign</source>
            <translation>IM 加密 &amp;&amp; 签名</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="195" />
            <source>Encrypt and sign the message as a single-line token for chat apps</source>
            <translation>将消息加密并签名为适用于聊天应用的单行令牌</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="250" />
            <source>instant messaging,message book,phrase,fingerprint,token</source>
            <translation>instant messaging,message book,phrase,fingerprint,token,即时通讯,消息簿,短语,指纹,令牌</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="73" />
            <source>Message Book Phrase</source>
            <translation>消息簿短语</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="77" />
            <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
            <translation>与一位朋友共享的长秘密。它让你的消息看起来像随机文本，因此没人能看出它们是PGP。你和朋友必须使用完全相同的短语。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="92" />
            <source>No phrase set. Messages use the built-in default book.</source>
            <translation>未设置短语。消息使用内置默认簿。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="108" />
            <source>Generate</source>
            <translation>生成</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="109" />
            <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
            <translation>创建新的随机短语。与朋友分享，以便你们使用相同的短语。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="120" />
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Show</source>
            <translation>显示</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="121" />
            <source>Show or hide the phrase.</source>
            <translation>显示或隐藏短语。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="125" />
            <location filename="../ImSettingsPage.cpp" line="177" />
            <source>Copy</source>
            <translation>复制</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="126" />
            <source>Copy the phrase to the clipboard.</source>
            <translation>将短语复制到剪贴板。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="130" />
            <source>Paste</source>
            <translation>粘贴</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="132" />
            <source>Replace the phrase with the one on the clipboard.</source>
            <translation>用剪贴板上的短语替换当前短语。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="138" />
            <source>Clear</source>
            <translation>清除</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="140" />
            <source>Remove the phrase and fall back to the default book.</source>
            <translation>移除短语并回退到默认口令本。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="162" />
            <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
            <translation>由您的短语生成的短代码。与您的朋友大声读出，以确保你们拥有相同的代码。与短语不同，此代码可以大声说出。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="179" />
            <source>Copy the fingerprint to the clipboard.</source>
            <translation>将指纹复制到剪贴板。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="191" />
            <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
            <translation>短语存储在加密缓存中，绝不会存储在设置文件中。请通过私密渠道发送给您的朋友。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Hide</source>
            <translation>隐藏</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="249" />
            <source>No phrase set. Using the built-in default.</source>
            <translation>未设置短语。使用内置默认值。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="250" />
            <source>Phrase set. %1 characters.</source>
            <translation>已设置短语。共 %1 个字符。</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="260" />
            <source>Calculating…</source>
            <translation>正在计算…</translation>
        </message>
    </context>
</TS>