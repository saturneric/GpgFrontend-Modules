<?xml version='1.0' encoding='utf-8'?>
<TS version="2.1">
    <context>
        <name>ModuleIM</name>
        <message>
            <location filename="../ImCodec.cpp" line="58" />
            <source>Encoding</source>
            <translation>Кодировка</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="59" />
            <source>Container Format</source>
            <translation>Формат контейнера</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="61" />
            <source>Message Book</source>
            <translation>Книга сообщений</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="62" />
            <source>Shared phrase (Argon2id)</source>
            <translation>Общая фраза (Argon2id)</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="63" />
            <source>Default, no shared phrase set</source>
            <translation>По умолчанию, общая фраза не задана</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="67" />
            <location filename="../ImSettingsPage.cpp" line="158" />
            <source>Book Fingerprint</source>
            <translation>Отпечаток книги</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Set a Phrase</source>
            <translation>Задать фразу</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Settings &gt; Instant Messaging</source>
            <translation>Настройки &gt; Мгновенные сообщения</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>OpenPGP Payload</source>
            <translation>Полезная нагрузка OpenPGP</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>%1 bytes</source>
            <translation>%1 байт</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>Token Length</source>
            <translation>Длина токена</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>%1 characters</source>
            <translation>%1 символов</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="82" />
            <source>Wire Overhead</source>
            <translation>Накладные расходы канала</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="87" />
            <location filename="../ImModule.cpp" line="249" />
            <source>Instant Messaging</source>
            <translation>Мгновенные сообщения</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="96" />
            <source>An Instant Messaging section followed by the OpenPGP result.</source>
            <translation>Раздел мгновенных сообщений, за которым следует результат OpenPGP.</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="126" />
            <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
            <translation>Это сообщение слишком длинное для отправки как мгновенное сообщение.

Зашифрованное сообщение имеет размер %1 байт, а формат мгновенных сообщений поддерживает не более %2. Сократите текст или отправьте его как обычное сообщение OpenPGP.</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="138" />
            <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
            <translation>Не удалось подготовить мгновенное сообщение: зашифрованное сообщение не удалось преобразовать в токен.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="85" />
            <location filename="../ImModule.cpp" line="98" />
            <source>Instant Message Token</source>
            <translation>Токен мгновенного сообщения</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="86" />
            <source>Recognise and unwrap instant messaging tokens before decrypting</source>
            <translation>Распознавать и распаковывать токены мгновенных сообщений перед расшифровкой</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="99" />
            <source>Wrap an encrypted message as an instant messaging token</source>
            <translation>Упаковать зашифрованное сообщение в токен мгновенного сообщения</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="134" />
            <source>No Message Book Phrase Set</source>
            <translation>Фраза книги сообщений не задана</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="135" />
            <source>You have not set a Message Book phrase.</source>
            <translation>Вы не задали фразу для книги сообщений.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="136" />
            <source>Instant messages are hidden using a shared "Message Book". Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
            <translation>Мгновенные сообщения скрываются с помощью общей «Книги сообщений». Без фразы GpgFrontend использует встроенную книгу по умолчанию, которая поставляется в каждой копии программы. Она скрывает формат от простого сканера, но любой, кто знает GpgFrontend, всё равно сможет распознать ваше сообщение.

Ваше сообщение в любом случае зашифровано OpenPGP; под вопросом только то, будет ли оно вообще распознано как зашифрованное.

Чтобы этого избежать, задайте фразу и поделитесь ею лично с получателем. Вы оба должны использовать одну и ту же фразу.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="148" />
            <source>Open Settings...</source>
            <translation>Открыть настройки...</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="150" />
            <source>Continue with Default</source>
            <translation>Продолжить с настройками по умолчанию</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="151" />
            <source>Continue, Don't Ask Again</source>
            <translation>Продолжить, больше не спрашивать</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="181" />
            <source>IM Encrypt</source>
            <translation>IM-шифрование</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="182" />
            <source>Encrypt the message as a single-line token for chat apps</source>
            <translation>Зашифровать сообщение как однострочный токен для мессенджеров</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="194" />
            <source>IM Encrypt &amp;&amp; Sign</source>
            <translation>IM-шифрование &amp;&amp; подписание</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="195" />
            <source>Encrypt and sign the message as a single-line token for chat apps</source>
            <translation>Зашифровать и подписать сообщение как однострочный токен для мессенджеров</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="250" />
            <source>instant messaging,message book,phrase,fingerprint,token</source>
            <translation>instant messaging,message book,phrase,fingerprint,token,мгновенные сообщения,книга сообщений,фраза,отпечаток,токен</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="73" />
            <source>Message Book Phrase</source>
            <translation>Фраза из книги сообщений</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="77" />
            <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
            <translation>Длинный секрет, которым вы делитесь с одним другом. Он делает ваши сообщения похожими на случайный текст, так что никто не сможет понять, что это PGP. Вы и ваш друг должны использовать одну и ту же фразу.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="92" />
            <source>No phrase set. Messages use the built-in default book.</source>
            <translation>Фраза не задана. Сообщения используют встроенную книгу по умолчанию.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="108" />
            <source>Generate</source>
            <translation>Сгенерировать</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="109" />
            <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
            <translation>Создать новую случайную фразу. Поделитесь ею с другом, чтобы вы оба использовали одну и ту же.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="120" />
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Show</source>
            <translation>Показывать</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="121" />
            <source>Show or hide the phrase.</source>
            <translation>Показать или скрыть фразу.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="125" />
            <location filename="../ImSettingsPage.cpp" line="177" />
            <source>Copy</source>
            <translation>Скопировать</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="126" />
            <source>Copy the phrase to the clipboard.</source>
            <translation>Скопировать фразу в буфер обмена.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="130" />
            <source>Paste</source>
            <translation>Вставить</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="132" />
            <source>Replace the phrase with the one on the clipboard.</source>
            <translation>Заменить фразу на ту, что в буфере обмена.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="138" />
            <source>Clear</source>
            <translation>Очистить</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="140" />
            <source>Remove the phrase and fall back to the default book.</source>
            <translation>Удалить фразу и вернуться к книге по умолчанию.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="162" />
            <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
            <translation>Короткий код, созданный из вашей фразы. Прочитайте его вслух вместе с другом, чтобы убедиться, что у вас обоих одна и та же фраза. В отличие от фразы, этот код можно произносить вслух.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="179" />
            <source>Copy the fingerprint to the clipboard.</source>
            <translation>Скопировать отпечаток в буфер обмена.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="191" />
            <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
            <translation>Фраза хранится в зашифрованном кеше, а не в файле настроек. Отправьте её другу по приватному каналу.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Hide</source>
            <translation>Скрыть</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="249" />
            <source>No phrase set. Using the built-in default.</source>
            <translation>Фраза не задана. Используется встроенная по умолчанию.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="250" />
            <source>Phrase set. %1 characters.</source>
            <translation>Фраза задана. %1 символов.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="260" />
            <source>Calculating…</source>
            <translation>Вычисление…</translation>
        </message>
    </context>
</TS>