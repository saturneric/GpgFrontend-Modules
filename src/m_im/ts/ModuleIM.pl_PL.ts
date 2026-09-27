<?xml version='1.0' encoding='utf-8'?>
<TS version="2.1">
    <context>
        <name>ModuleIM</name>
        <message>
            <location filename="../ImCodec.cpp" line="58" />
            <source>Encoding</source>
            <translation>Kodowanie</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="59" />
            <source>Container Format</source>
            <translation>Format kontenera</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="61" />
            <source>Message Book</source>
            <translation>Książka Wiadomości</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="62" />
            <source>Shared phrase (Argon2id)</source>
            <translation>Wspólna fraza (Argon2id)</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="63" />
            <source>Default, no shared phrase set</source>
            <translation>Domyślna, nie ustawiono wspólnej frazy</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="67" />
            <location filename="../ImSettingsPage.cpp" line="158" />
            <source>Book Fingerprint</source>
            <translation>Odcisk książki</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Set a Phrase</source>
            <translation>Ustaw frazę</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="69" />
            <source>Settings &gt; Instant Messaging</source>
            <translation>Ustawienia &gt; Komunikatory</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>OpenPGP Payload</source>
            <translation>Dane OpenPGP</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="72" />
            <source>%1 bytes</source>
            <translation>%1 bajtów</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>Token Length</source>
            <translation>Długość tokena</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="75" />
            <source>%1 characters</source>
            <translation>%1 znaków</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="82" />
            <source>Wire Overhead</source>
            <translation>Narzut transmisji</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="87" />
            <location filename="../ImModule.cpp" line="249" />
            <source>Instant Messaging</source>
            <translation>Komunikatory</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="96" />
            <source>An Instant Messaging section followed by the OpenPGP result.</source>
            <translation>Sekcja komunikatora, a po niej wynik OpenPGP.</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="126" />
            <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
            <translation>Ta wiadomość jest za długa, aby wysłać ją jako wiadomość komunikatora.

Zaszyfrowana wiadomość ma %1 bajtów, a format komunikatora przenosi maksymalnie %2. Skróć tekst albo wyślij ją zamiast tego jako zwykłą wiadomość OpenPGP.</translation>
        </message>
        <message>
            <location filename="../ImCodec.cpp" line="138" />
            <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
            <translation>Nie udało się przygotować wiadomości komunikatora: zaszyfrowanej wiadomości nie udało się przekształcić w token.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="85" />
            <location filename="../ImModule.cpp" line="98" />
            <source>Instant Message Token</source>
            <translation>Token komunikatora</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="86" />
            <source>Recognise and unwrap instant messaging tokens before decrypting</source>
            <translation>Rozpoznaj i rozpakuj tokeny komunikatora przed odszyfrowaniem</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="99" />
            <source>Wrap an encrypted message as an instant messaging token</source>
            <translation>Opakuj zaszyfrowaną wiadomość jako token komunikatora</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="134" />
            <source>No Message Book Phrase Set</source>
            <translation>Brak frazy Książki Wiadomości</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="135" />
            <source>You have not set a Message Book phrase.</source>
            <translation>Fraza Książki Wiadomości nie została ustawiona.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="136" />
            <source>Instant messages are hidden using a shared "Message Book". Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
            <translation>Wiadomości na komunikatorach są ukrywane za pomocą wspólnej „Książki Wiadomości”. Bez frazy GpgFrontend sięga po wbudowaną domyślną książkę, a ta książka jest dostarczana w każdej kopii programu. Ukrywa ona format przed prostym skanerem, ale każdy, kto zna GpgFrontend, nadal rozpozna Twoją wiadomość za to, czym jest.

Twoja wiadomość jest tak czy inaczej zaszyfrowana OpenPGP; chodzi tu wyłącznie o to, czy w ogóle da się rozpoznać, że to zaszyfrowana wiadomość.

Aby to uzyskać, ustaw frazę i przekaż ją prywatnie osobie, do której piszesz. Oboje musicie używać dokładnie tej samej frazy.</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="148" />
            <source>Open Settings...</source>
            <translation>Otwórz ustawienia...</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="150" />
            <source>Continue with Default</source>
            <translation>Kontynuuj z domyślną</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="151" />
            <source>Continue, Don't Ask Again</source>
            <translation>Kontynuuj, nie pytaj ponownie</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="181" />
            <source>IM Encrypt</source>
            <translation>Zaszyfruj do komunikatora</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="182" />
            <source>Encrypt the message as a single-line token for chat apps</source>
            <translation>Zaszyfruj wiadomość jako jednowierszowy token dla komunikatorów</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="194" />
            <source>IM Encrypt &amp;&amp; Sign</source>
            <translation>Zaszyfruj i podpisz do komunikatora</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="195" />
            <source>Encrypt and sign the message as a single-line token for chat apps</source>
            <translation>Zaszyfruj i podpisz wiadomość jako jednowierszowy token dla komunikatorów</translation>
        </message>
        <message>
            <location filename="../ImModule.cpp" line="250" />
            <source>instant messaging,message book,phrase,fingerprint,token</source>
            <translation>instant messaging,message book,phrase,fingerprint,token,komunikator,księga wiadomości,fraza,odcisk</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="73" />
            <source>Message Book Phrase</source>
            <translation>Fraza Książki Wiadomości</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="77" />
            <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
            <translation>Długi sekret, który dzielisz z jednym znajomym. Sprawia, że Twoje wiadomości wyglądają jak losowy tekst, więc nikt nie pozna, że to PGP. Ty i Twój znajomy musicie używać dokładnie tej samej frazy.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="92" />
            <source>No phrase set. Messages use the built-in default book.</source>
            <translation>Nie ustawiono frazy. Wiadomości używają wbudowanej domyślnej książki.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="108" />
            <source>Generate</source>
            <translation>Wygeneruj</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="109" />
            <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
            <translation>Utwórz nową losową frazę. Przekaż ją znajomemu, żebyście używali tej samej.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="120" />
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Show</source>
            <translation>Pokaż</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="121" />
            <source>Show or hide the phrase.</source>
            <translation>Pokaż lub ukryj frazę.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="125" />
            <location filename="../ImSettingsPage.cpp" line="177" />
            <source>Copy</source>
            <translation>Skopiuj</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="126" />
            <source>Copy the phrase to the clipboard.</source>
            <translation>Skopiuj frazę do schowka.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="130" />
            <source>Paste</source>
            <translation>Wklej</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="132" />
            <source>Replace the phrase with the one on the clipboard.</source>
            <translation>Zastąp frazę tą ze schowka.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="138" />
            <source>Clear</source>
            <translation>Wyczyść</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="140" />
            <source>Remove the phrase and fall back to the default book.</source>
            <translation>Usuń frazę i wróć do domyślnej książki.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="162" />
            <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
            <translation>Krótki kod wygenerowany z Twojej frazy. Odczytaj go znajomemu, aby upewnić się, że macie ten sam. W przeciwieństwie do frazy, ten kod można bezpiecznie wypowiedzieć na głos.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="179" />
            <source>Copy the fingerprint to the clipboard.</source>
            <translation>Skopiuj odcisk do schowka.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="191" />
            <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
            <translation>Fraza jest przechowywana w zaszyfrowanej pamięci podręcznej, nigdy w pliku ustawień. Prześlij ją znajomemu prywatnym kanałem.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="228" />
            <source>Hide</source>
            <translation>Ukryj</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="249" />
            <source>No phrase set. Using the built-in default.</source>
            <translation>Nie ustawiono frazy. Używana jest wbudowana domyślna książka.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="250" />
            <source>Phrase set. %1 characters.</source>
            <translation>Fraza ustawiona. Znaków: %1.</translation>
        </message>
        <message>
            <location filename="../ImSettingsPage.cpp" line="260" />
            <source>Calculating…</source>
            <translation>Obliczanie…</translation>
        </message>
    </context>
</TS>