<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="tr_TR">
<context>
    <name>ModuleIM</name>
    <message>
        <location filename="../ImCodec.cpp" line="58"/>
        <source>Encoding</source>
        <translation>Kodlama</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="59"/>
        <source>Container Format</source>
        <translation>Kapsayıcı biçimi</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="61"/>
        <source>Message Book</source>
        <translation>Mesaj Kitabı</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="62"/>
        <source>Shared phrase (Argon2id)</source>
        <translation>Paylaşılan ifade (Argon2id)</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="63"/>
        <source>Default, no shared phrase set</source>
        <translation>Varsayılan, paylaşılan ifade belirlenmedi</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="67"/>
        <location filename="../ImSettingsPage.cpp" line="158"/>
        <source>Book Fingerprint</source>
        <translation>Kitap parmak izi</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="69"/>
        <source>Set a Phrase</source>
        <translation>İfade belirle</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="69"/>
        <source>Settings &gt; Instant Messaging</source>
        <translation>Ayarlar &gt; Anlık Mesajlaşma</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="72"/>
        <source>OpenPGP Payload</source>
        <translation>OpenPGP veri yükü</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="72"/>
        <source>%1 bytes</source>
        <translation>%1 bayt</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="75"/>
        <source>Token Length</source>
        <translation>Belirteç uzunluğu</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="75"/>
        <source>%1 characters</source>
        <translation>%1 karakter</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="82"/>
        <source>Wire Overhead</source>
        <translation>İletim ek yükü</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="87"/>
        <location filename="../ImModule.cpp" line="253"/>
        <source>Instant Messaging</source>
        <translation>Anlık Mesajlaşma</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="96"/>
        <source>An Instant Messaging section followed by the OpenPGP result.</source>
        <translation>Bir Anlık Mesajlaşma bölümü, ardından OpenPGP sonucu.</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="126"/>
        <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
        <translation>Bu mesaj, anlık mesaj olarak gönderilemeyecek kadar uzun.

Şifrelenmiş mesaj %1 bayttır ve anlık mesajlaşma biçimi en fazla %2 bayt taşıyabilir. Metni kısaltın veya bunun yerine normal bir OpenPGP mesajı olarak gönderin.</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="138"/>
        <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
        <translation>Anlık mesaj hazırlanamadı: şifrelenmiş mesaj bir belirtece dönüştürülemedi.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="89"/>
        <location filename="../ImModule.cpp" line="102"/>
        <source>Instant Message Token</source>
        <translation>Anlık Mesaj Belirteci</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="90"/>
        <source>Recognise and unwrap instant messaging tokens before decrypting</source>
        <translation>Şifre çözmeden önce anlık mesajlaşma belirteçlerini tanıyın ve açın</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="103"/>
        <source>Wrap an encrypted message as an instant messaging token</source>
        <translation>Şifrelenmiş bir mesajı anlık mesajlaşma belirteci olarak sarın</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="138"/>
        <source>No Message Book Phrase Set</source>
        <translation>Mesaj Kitabı İfadesi Belirlenmedi</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="139"/>
        <source>You have not set a Message Book phrase.</source>
        <translation>Bir Mesaj Kitabı ifadesi belirlemediniz.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="140"/>
        <source>Instant messages are hidden using a shared &quot;Message Book&quot;. Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
        <translation>Anlık mesajlar, paylaşılan bir &quot;Mesaj Kitabı&quot; ile gizlenir. İfade belirlemezseniz GpgFrontend yerleşik varsayılan kitaba döner. Bu kitap programın her kopyasında bulunur. Biçimi basit bir tarayıcıdan gizler, ancak GpgFrontend&apos;i bilen herkes mesajınızın ne olduğunu yine de anlayabilir.

Mesajınız her iki durumda da OpenPGP ile şifrelenir. Burada söz konusu olan yalnızca mesajın şifreli bir ileti olarak tanınıp tanınmayacağıdır.

Bunu sağlamak için bir ifade belirleyin ve yazdığınız kişiyle özel olarak paylaşın. İkiniz de birebir aynı ifadeyi kullanmalısınız.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="152"/>
        <source>Open Settings...</source>
        <translation>Ayarları Aç...</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="154"/>
        <source>Continue with Default</source>
        <translation>Varsayılanla Devam Et</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="155"/>
        <source>Continue, Don&apos;t Ask Again</source>
        <translation>Devam Et, Bir Daha Sorma</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="185"/>
        <source>IM Encrypt</source>
        <translation>IM Şifrele</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="186"/>
        <source>Encrypt the message as a single-line token for chat apps</source>
        <translation>Mesajı sohbet uygulamaları için tek satırlık bir belirteç olarak şifreleyin</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="198"/>
        <source>IM Encrypt &amp;&amp; Sign</source>
        <translation>IM Şifrele &amp;&amp; İmzala</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="199"/>
        <source>Encrypt and sign the message as a single-line token for chat apps</source>
        <translation>Mesajı sohbet uygulamaları için tek satırlık bir belirteç olarak şifreleyin ve imzalayın</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="254"/>
        <source>instant messaging,message book,phrase,fingerprint,token</source>
        <translation>anlık mesajlaşma,mesaj kitabı,ifade,parmak izi,belirteç,instant messaging,message book,phrase,fingerprint,token</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="73"/>
        <source>Message Book Phrase</source>
        <translation>Mesaj Kitabı İfadesi</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="77"/>
        <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
        <translation>Tek bir arkadaşınızla paylaştığınız uzun bir sır. Mesajlarınızın rastgele metin gibi görünmesini sağlar; böylece kimse bunların PGP olduğunu anlayamaz. Siz ve arkadaşınız birebir aynı ifadeyi kullanmalısınız.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="92"/>
        <source>No phrase set. Messages use the built-in default book.</source>
        <translation>İfade belirlenmedi. Mesajlar yerleşik varsayılan kitabı kullanır.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="108"/>
        <source>Generate</source>
        <translation>Oluştur</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="109"/>
        <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
        <translation>Yeni bir rastgele ifade oluşturun. İkinizin de aynı ifadeyi kullanması için onu arkadaşınızla paylaşın.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="120"/>
        <location filename="../ImSettingsPage.cpp" line="228"/>
        <source>Show</source>
        <translation>Göster</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="121"/>
        <source>Show or hide the phrase.</source>
        <translation>İfadeyi gösterin veya gizleyin.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="125"/>
        <location filename="../ImSettingsPage.cpp" line="177"/>
        <source>Copy</source>
        <translation>Kopyala</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="126"/>
        <source>Copy the phrase to the clipboard.</source>
        <translation>İfadeyi panoya kopyalayın.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="130"/>
        <source>Paste</source>
        <translation>Yapıştır</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="132"/>
        <source>Replace the phrase with the one on the clipboard.</source>
        <translation>İfadeyi panodaki ifadeyle değiştirin.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="138"/>
        <source>Clear</source>
        <translation>Temizle</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="140"/>
        <source>Remove the phrase and fall back to the default book.</source>
        <translation>İfadeyi kaldırın ve varsayılan kitaba dönün.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="162"/>
        <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
        <translation>İfadenizden türetilen kısa bir kod. İkinizin de aynı ifadeye sahip olduğunu doğrulamak için arkadaşınızla sesli okuyun. İfadenin aksine bu kodu yüksek sesle söylemek güvenlidir.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="179"/>
        <source>Copy the fingerprint to the clipboard.</source>
        <translation>Parmak izini panoya kopyalayın.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="191"/>
        <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
        <translation>İfade şifreli önbellekte saklanır, ayar dosyasında asla saklanmaz. Arkadaşınıza özel bir kanaldan gönderin.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="228"/>
        <source>Hide</source>
        <translation>Gizle</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="249"/>
        <source>No phrase set. Using the built-in default.</source>
        <translation>İfade belirlenmedi. Yerleşik varsayılan kullanılıyor.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="250"/>
        <source>Phrase set. %1 characters.</source>
        <translation>İfade belirlendi. %1 karakter.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="260"/>
        <source>Calculating…</source>
        <translation>Hesaplanıyor…</translation>
    </message>
</context>
</TS>
