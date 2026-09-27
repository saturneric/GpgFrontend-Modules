<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1">
<context>
    <name>ModuleIM</name>
    <message>
        <location filename="../ImCodec.cpp" line="58"/>
        <source>Encoding</source>
        <translation>Codificación</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="59"/>
        <source>Container Format</source>
        <translation>Formato de contenedor</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="61"/>
        <source>Message Book</source>
        <translation>Libro de mensajes</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="62"/>
        <source>Shared phrase (Argon2id)</source>
        <translation>Frase compartida (Argon2id)</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="63"/>
        <source>Default, no shared phrase set</source>
        <translation>Predeterminado, sin frase compartida</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="67"/>
        <location filename="../ImSettingsPage.cpp" line="158"/>
        <source>Book Fingerprint</source>
        <translation>Huella del libro</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="69"/>
        <source>Set a Phrase</source>
        <translation>Establecer una frase</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="69"/>
        <source>Settings &gt; Instant Messaging</source>
        <translation>Ajustes &gt; Mensajería instantánea</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="72"/>
        <source>OpenPGP Payload</source>
        <translation>Carga útil OpenPGP</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="72"/>
        <source>%1 bytes</source>
        <translation>%1 bytes</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="75"/>
        <source>Token Length</source>
        <translation>Longitud del token</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="75"/>
        <source>%1 characters</source>
        <translation>%1 caracteres</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="82"/>
        <source>Wire Overhead</source>
        <translation>Sobrecarga de red</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="87"/>
        <location filename="../ImModule.cpp" line="249"/>
        <source>Instant Messaging</source>
        <translation>Mensajería instantánea</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="96"/>
        <source>An Instant Messaging section followed by the OpenPGP result.</source>
        <translation>Una sección de Mensajería Instantánea seguida del resultado OpenPGP.</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="126"/>
        <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
        <translation>Este mensaje es demasiado largo para enviarlo como mensaje instantáneo.

El mensaje cifrado tiene %1 bytes y el formato de mensajería instantánea admite como máximo %2. Acorte el texto o envíelo como un mensaje OpenPGP normal.</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="138"/>
        <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
        <translation>Error al preparar el mensaje instantáneo: no se pudo convertir el mensaje cifrado en un token.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="85"/>
        <location filename="../ImModule.cpp" line="98"/>
        <source>Instant Message Token</source>
        <translation>Token de mensajería instantánea</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="86"/>
        <source>Recognise and unwrap instant messaging tokens before decrypting</source>
        <translation>Reconocer y desenvolver los tokens de mensajería instantánea antes de descifrar</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="99"/>
        <source>Wrap an encrypted message as an instant messaging token</source>
        <translation>Envolver un mensaje cifrado como un token de mensajería instantánea</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="134"/>
        <source>No Message Book Phrase Set</source>
        <translation>No se ha establecido ninguna frase de Message Book</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="135"/>
        <source>You have not set a Message Book phrase.</source>
        <translation>No ha establecido una frase de Message Book.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="136"/>
        <source>Instant messages are hidden using a shared &quot;Message Book&quot;. Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
        <translation>Los mensajes instantáneos se ocultan usando un &quot;Libro de mensajes&quot; compartido. Sin una frase, GpgFrontend recurre al libro predeterminado integrado, y ese libro se incluye en cada copia del programa. Oculta el formato de un escáner simple, pero cualquiera que conozca GpgFrontend aún puede reconocer su mensaje por lo que es.

Su mensaje está cifrado con OpenPGP de cualquier manera; lo que está en juego aquí es solo si es reconocible como un mensaje cifrado en absoluto.

Para lograrlo, establezca una frase y compártala de forma privada con la persona a la que le escribe. Ambos deben usar exactamente la misma.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="148"/>
        <source>Open Settings...</source>
        <translation>Abrir ajustes...</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="150"/>
        <source>Continue with Default</source>
        <translation>Continuar con el predeterminado</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="151"/>
        <source>Continue, Don&apos;t Ask Again</source>
        <translation>Continuar, no preguntar de nuevo</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="181"/>
        <source>IM Encrypt</source>
        <translation>Cifrar IM</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="182"/>
        <source>Encrypt the message as a single-line token for chat apps</source>
        <translation>Cifrar el mensaje como un token de una sola línea para aplicaciones de chat</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="194"/>
        <source>IM Encrypt &amp;&amp; Sign</source>
        <translation>Cifrar IM &amp;&amp; Firmar</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="195"/>
        <source>Encrypt and sign the message as a single-line token for chat apps</source>
        <translation>Cifrar y firmar el mensaje como un token de una sola línea para aplicaciones de chat</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="250"/>
        <source>instant messaging,message book,phrase,fingerprint,token</source>
        <translation>instant messaging,message book,phrase,fingerprint,token,mensajería instantánea,libro de mensajes,frase,huella</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="73"/>
        <source>Message Book Phrase</source>
        <translation>Frase del libro de mensajes</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="77"/>
        <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
        <translation>Un secreto largo que compartes con un amigo. Hace que tus mensajes parezcan texto aleatorio, por lo que nadie puede saber que son PGP. Tú y tu amigo deben usar exactamente la misma frase.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="92"/>
        <source>No phrase set. Messages use the built-in default book.</source>
        <translation>No se ha establecido ninguna frase. Los mensajes usan el libro predeterminado incorporado.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="108"/>
        <source>Generate</source>
        <translation>Generar</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="109"/>
        <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
        <translation>Crea una nueva frase aleatoria. Compártela con tu amigo para que ambos usen la misma.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="120"/>
        <location filename="../ImSettingsPage.cpp" line="228"/>
        <source>Show</source>
        <translation>Mostrar</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="121"/>
        <source>Show or hide the phrase.</source>
        <translation>Muestra u oculta la frase.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="125"/>
        <location filename="../ImSettingsPage.cpp" line="177"/>
        <source>Copy</source>
        <translation>Copiar</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="126"/>
        <source>Copy the phrase to the clipboard.</source>
        <translation>Copia la frase al portapapeles.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="130"/>
        <source>Paste</source>
        <translation>Pegar</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="132"/>
        <source>Replace the phrase with the one on the clipboard.</source>
        <translation>Reemplaza la frase con la del portapapeles.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="138"/>
        <source>Clear</source>
        <translation>Limpiar</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="140"/>
        <source>Remove the phrase and fall back to the default book.</source>
        <translation>Eliminar la frase y volver al libro predeterminado.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="162"/>
        <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
        <translation>Un código corto generado a partir de tu frase. Léelo en voz alta con tu amigo para asegurarte de que ambos tienen la misma. A diferencia de la frase, este código es seguro decirlo en voz alta.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="179"/>
        <source>Copy the fingerprint to the clipboard.</source>
        <translation>Copiar la huella al portapapeles.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="191"/>
        <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
        <translation>La frase se almacena en la caché cifrada, nunca en el archivo de configuración. Envíala a tu amigo por un canal privado.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="228"/>
        <source>Hide</source>
        <translation>Ocultar</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="249"/>
        <source>No phrase set. Using the built-in default.</source>
        <translation>No se ha establecido ninguna frase. Usando el valor predeterminado integrado.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="250"/>
        <source>Phrase set. %1 characters.</source>
        <translation>Frase establecida. %1 caracteres.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="260"/>
        <source>Calculating…</source>
        <translation>Calculando…</translation>
    </message>
</context>
</TS>
