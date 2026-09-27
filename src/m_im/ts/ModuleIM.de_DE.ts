<?xml version='1.0' encoding='utf-8'?>
<TS version="2.1">
    <context>
        <name>ModuleIM</name>
        <message>
            <location filename="../ImCodec.cpp" line="58" />
            <source>Encoding</source>
            <translation>Kodierung</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="59" />
            <source>Container Format</source>
            <translation>Containerformat</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="61" />
            <source>Message Book</source>
            <translation>Nachrichtenbuch</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="62" />
            <source>Shared phrase (Argon2id)</source>
            <translation>Geteilter Ausdruck (Argon2id)</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="63" />
            <source>Default, no shared phrase set</source>
            <translation>Standard, keine gemeinsame Phrase festgelegt</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="67" />
            <location filename="../ImSettingsPage.cpp" line="158" />
            <source>Book Fingerprint</source>
            <translation>Buch-Fingerabdruck</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Set a Phrase</source>
            <translation>Eine Phrase festlegen</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Settings &gt; Instant Messaging</source>
            <translation>Einstellungen &gt; Instant Messaging</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>OpenPGP Payload</source>
            <translation>OpenPGP-Nutzlast</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>%1 bytes</source>
            <translation>%1 Bytes</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>Token Length</source>
            <translation>Token-Länge</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>%1 characters</source>
            <translation>%1 Zeichen</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="82" />
            <source>Wire Overhead</source>
            <translation>Leitungs-Overhead</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="87" />
            <location filename="../ImModule.cpp" line="249" />
            <source>Instant Messaging</source>
            <translation>Instant Messaging</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="96" />
            <source>An Instant Messaging section followed by the OpenPGP result.</source>
            <translation>Ein Sofortnachrichten-Abschnitt, gefolgt vom OpenPGP-Ergebnis.</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="126" />
            <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
            <translation>Diese Nachricht ist zu lang, um als Sofortnachricht gesendet zu werden.

Die verschlüsselte Nachricht ist %1 Bytes groß, und das Sofortnachrichten-Format unterstützt maximal %2. Kürzen Sie den Text oder senden Sie ihn stattdessen als normale OpenPGP-Nachricht.</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="138" />
            <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
            <translation>Fehler beim Vorbereiten der Sofortnachricht: Die verschlüsselte Nachricht konnte nicht in ein Token umgewandelt werden.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="85" />
            <location filename="../ImModule.cpp" line="98" />
            <source>Instant Message Token</source>
            <translation>Instant-Messaging-Token</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="86" />
            <source>Recognise and unwrap instant messaging tokens before decrypting</source>
            <translation>Instant-Messaging-Token vor dem Entschlüsseln erkennen und entpacken</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="99" />
            <source>Wrap an encrypted message as an instant messaging token</source>
            <translation>Eine verschlüsselte Nachricht als Instant-Messaging-Token verpacken</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="134" />
            <source>No Message Book Phrase Set</source>
            <translation>Keine Nachrichtenbuch-Phrase festgelegt</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="135" />
            <source>You have not set a Message Book phrase.</source>
            <translation>Sie haben keine Nachrichtenbuch-Phrase festgelegt.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="136" />
            <source>Instant messages are hidden using a shared "Message Book". Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
            <translation>Sofortnachrichten werden mithilfe eines gemeinsamen "Nachrichtenbuchs" verborgen. Ohne eine Phrase verwendet GpgFrontend das integrierte Standardbuch, das in jeder Kopie des Programms enthalten ist. Es verbirgt das Format vor einem einfachen Scanner, aber jeder, der GpgFrontend kennt, kann Ihre Nachricht dennoch als solche erkennen.

Ihre Nachricht ist auf jeden Fall OpenPGP-verschlüsselt; es geht hier nur darum, ob sie überhaupt als verschlüsselte Nachricht erkennbar ist.

Um dies zu erreichen, legen Sie eine Phrase fest und teilen Sie diese privat mit der Person, mit der Sie schreiben. Sie müssen beide genau dieselbe verwenden.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="148" />
            <source>Open Settings...</source>
            <translation>Einstellungen öffnen...</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="150" />
            <source>Continue with Default</source>
            <translation>Mit Standard fortfahren</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="151" />
            <source>Continue, Don't Ask Again</source>
            <translation>Fortfahren, nicht erneut fragen</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="181" />
            <source>IM Encrypt</source>
            <translation>IM-Verschlüsselung</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="182" />
            <source>Encrypt the message as a single-line token for chat apps</source>
            <translation>Die Nachricht als einzeiliges Token für Chat-Apps verschlüsseln</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="194" />
            <source>IM Encrypt &amp;&amp; Sign</source>
            <translation>IM-Verschlüsselung &amp;&amp; Signieren</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="195" />
            <source>Encrypt and sign the message as a single-line token for chat apps</source>
            <translation>Die Nachricht als einzeiliges Token für Chat-Apps verschlüsseln und signieren</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="250" />
            <source>instant messaging,message book,phrase,fingerprint,token</source>
            <translation>instant messaging,message book,phrase,fingerprint,token,Nachrichtenbuch,Phrase,Fingerabdruck</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="73" />
            <source>Message Book Phrase</source>
            <translation>Nachrichtenbuch-Phrase</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="77" />
            <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
            <translation>Ein langes Geheimnis, das du mit einem Freund teilst. Es lässt deine Nachrichten wie zufälligen Text aussehen, sodass niemand erkennen kann, dass es sich um PGP handelt. Du und dein Freund müsst genau dieselbe Phrase verwenden.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="92" />
            <source>No phrase set. Messages use the built-in default book.</source>
            <translation>Keine Phrase festgelegt. Nachrichten verwenden das integrierte Standardbuch.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="108" />
            <source>Generate</source>
            <translation>Erstellen</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="109" />
            <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
            <translation>Erstelle eine neue zufällige Phrase. Teile sie mit deinem Freund, damit ihr beide dieselbe verwendet.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="120" />
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Show</source>
            <translation>Anzeigen</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="121" />
            <source>Show or hide the phrase.</source>
            <translation>Phrase anzeigen oder ausblenden.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="125" />
            <location filename="../ImSettingsPage.cpp" line="177" />
            <source>Copy</source>
            <translation>Kopieren</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="126" />
            <source>Copy the phrase to the clipboard.</source>
            <translation>Phrase in die Zwischenablage kopieren.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="130" />
            <source>Paste</source>
            <translation>Einfügen</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="132" />
            <source>Replace the phrase with the one on the clipboard.</source>
            <translation>Phrase durch die in der Zwischenablage ersetzen.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="138" />
            <source>Clear</source>
            <translation>Löschen</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="140" />
            <source>Remove the phrase and fall back to the default book.</source>
            <translation>Passphrase entfernen und auf das Standardbuch zurückfallen.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="162" />
            <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
            <translation>Ein kurzer Code, der aus Ihrer Passphrase erstellt wurde. Lesen Sie ihn Ihrem Freund vor, um sicherzustellen, dass beide dieselbe Passphrase verwenden. Anders als die Passphrase kann dieser Code bedenkenlos laut ausgesprochen werden.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="179" />
            <source>Copy the fingerprint to the clipboard.</source>
            <translation>Fingerabdruck in die Zwischenablage kopieren.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="191" />
            <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
            <translation>Die Passphrase wird im verschlüsselten Cache gespeichert, niemals in der Einstellungsdatei. Senden Sie sie über einen privaten Kanal an Ihren Freund.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Hide</source>
            <translation>Ausblenden</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="249" />
            <source>No phrase set. Using the built-in default.</source>
            <translation>Keine Passphrase festgelegt. Standard wird verwendet.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="250" />
            <source>Phrase set. %1 characters.</source>
            <translation>Passphrase festgelegt. %1 Zeichen.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="260" />
            <source>Calculating…</source>
            <translation>Berechne…</translation>
        </message>
    </context>
</TS>