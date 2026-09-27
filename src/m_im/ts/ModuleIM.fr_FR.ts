<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1">
<context>
    <name>ModuleIM</name>
    <message>
        <location filename="../ImCodec.cpp" line="58"/>
        <source>Encoding</source>
        <translation>Encodage</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="59"/>
        <source>Container Format</source>
        <translation>Format de conteneur</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="61"/>
        <source>Message Book</source>
        <translation>Carnet de messages</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="62"/>
        <source>Shared phrase (Argon2id)</source>
        <translation>Phrase partagée (Argon2id)</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="63"/>
        <source>Default, no shared phrase set</source>
        <translation>Par défaut, aucune phrase partagée définie</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="67"/>
        <location filename="../ImSettingsPage.cpp" line="158"/>
        <source>Book Fingerprint</source>
        <translation>Empreinte du carnet</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="69"/>
        <source>Set a Phrase</source>
        <translation>Définir une phrase</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="69"/>
        <source>Settings &gt; Instant Messaging</source>
        <translation>Paramètres &gt; Messagerie instantanée</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="72"/>
        <source>OpenPGP Payload</source>
        <translation>Charge utile OpenPGP</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="72"/>
        <source>%1 bytes</source>
        <translation>%1 octets</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="75"/>
        <source>Token Length</source>
        <translation>Longueur du jeton</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="75"/>
        <source>%1 characters</source>
        <translation>%1 caractères</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="82"/>
        <source>Wire Overhead</source>
        <translation>Surcharge réseau</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="87"/>
        <location filename="../ImModule.cpp" line="249"/>
        <source>Instant Messaging</source>
        <translation>Messagerie instantanée</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="96"/>
        <source>An Instant Messaging section followed by the OpenPGP result.</source>
        <translation>Une section de messagerie instantanée suivie du résultat OpenPGP.</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="126"/>
        <source>This message is too long to send as an instant message.

The encrypted message is %1 bytes, and the instant-messaging format carries at most %2. Shorten the text, or send it as a normal OpenPGP message instead.</source>
        <translation>Ce message est trop long pour être envoyé en tant que message instantané.

Le message chiffré fait %1 octets, et le format de messagerie instantanée supporte au maximum %2. Raccourcissez le texte ou envoyez-le plutôt en tant que message OpenPGP normal.</translation>
    </message>
    <message>
        <location filename="../ImCodec.cpp" line="138"/>
        <source>Failed to prepare the instant message: the encrypted message could not be converted into a token.</source>
        <translation>Échec de la préparation du message instantané : le message chiffré n&apos;a pas pu être converti en jeton.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="85"/>
        <location filename="../ImModule.cpp" line="98"/>
        <source>Instant Message Token</source>
        <translation>Jeton de messagerie instantanée</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="86"/>
        <source>Recognise and unwrap instant messaging tokens before decrypting</source>
        <translation>Reconnaître et déballer les jetons de messagerie instantanée avant le déchiffrement</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="99"/>
        <source>Wrap an encrypted message as an instant messaging token</source>
        <translation>Envelopper un message chiffré sous forme de jeton de messagerie instantanée</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="134"/>
        <source>No Message Book Phrase Set</source>
        <translation>Aucune phrase de carnet de messages définie</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="135"/>
        <source>You have not set a Message Book phrase.</source>
        <translation>Vous n&apos;avez pas défini de phrase de carnet de messages.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="136"/>
        <source>Instant messages are hidden using a shared &quot;Message Book&quot;. Without a phrase, GpgFrontend falls back to the built-in default book and that book ships in every copy of the program. It hides the format from a simple scanner, but anyone who knows GpgFrontend can still recognise your message for what it is.

Your message is OpenPGP-encrypted either way; what is at stake here is only whether it is recognisable as an encrypted message at all.

To get that, set a phrase and share it privately with the person you are writing to. You must both use exactly the same one.</source>
        <translation>Les messages instantanés sont masqués à l&apos;aide d&apos;un « Carnet de messages » partagé. Sans phrase, GpgFrontend utilise le carnet par défaut intégré, et ce carnet est fourni dans chaque copie du programme. Il masque le format aux yeux d&apos;un simple scanner, mais toute personne connaissant GpgFrontend peut toujours reconnaître votre message pour ce qu&apos;il est.

Votre message est chiffré en OpenPGP de toute façon ; ce qui est en jeu ici est uniquement de savoir s&apos;il est reconnaissable en tant que message chiffré.

Pour y parvenir, définissez une phrase et partagez-la en privé avec votre correspondant. Vous devez tous deux utiliser exactement la même.</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="148"/>
        <source>Open Settings...</source>
        <translation>Ouvrir les paramètres...</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="150"/>
        <source>Continue with Default</source>
        <translation>Continuer avec le carnet par défaut</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="151"/>
        <source>Continue, Don&apos;t Ask Again</source>
        <translation>Continuer, ne plus demander</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="181"/>
        <source>IM Encrypt</source>
        <translation>Chiffrer IM</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="182"/>
        <source>Encrypt the message as a single-line token for chat apps</source>
        <translation>Chiffrer le message sous forme de jeton d’une seule ligne pour les applications de discussion</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="194"/>
        <source>IM Encrypt &amp;&amp; Sign</source>
        <translation>Chiffrer IM &amp;&amp; Signer</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="195"/>
        <source>Encrypt and sign the message as a single-line token for chat apps</source>
        <translation>Chiffrer et signer le message sous forme de jeton d’une seule ligne pour les applications de discussion</translation>
    </message>
    <message>
        <location filename="../ImModule.cpp" line="250"/>
        <source>instant messaging,message book,phrase,fingerprint,token</source>
        <translation>instant messaging,message book,phrase,fingerprint,token,messagerie instantanée,carnet de messages,empreinte,jeton</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="73"/>
        <source>Message Book Phrase</source>
        <translation>Phrase du livre de messages</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="77"/>
        <source>A long secret you share with one friend. It makes your messages look like random text, so nobody can tell they are PGP at all. You and your friend must use exactly the same phrase.</source>
        <translation>Un long secret que vous partagez avec un ami. Il rend vos messages semblables à du texte aléatoire, de sorte que personne ne puisse dire qu&apos;il s&apos;agit de PGP. Vous et votre ami devez utiliser exactement la même phrase.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="92"/>
        <source>No phrase set. Messages use the built-in default book.</source>
        <translation>Aucune phrase définie. Les messages utilisent le livre par défaut intégré.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="108"/>
        <source>Generate</source>
        <translation>Générer</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="109"/>
        <source>Create a new random phrase. Share it with your friend so you both use the same one.</source>
        <translation>Créez une nouvelle phrase aléatoire. Partagez-la avec votre ami pour que vous utilisiez tous les deux la même.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="120"/>
        <location filename="../ImSettingsPage.cpp" line="228"/>
        <source>Show</source>
        <translation>Afficher</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="121"/>
        <source>Show or hide the phrase.</source>
        <translation>Afficher ou masquer la phrase.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="125"/>
        <location filename="../ImSettingsPage.cpp" line="177"/>
        <source>Copy</source>
        <translation>Copier</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="126"/>
        <source>Copy the phrase to the clipboard.</source>
        <translation>Copier la phrase dans le presse-papiers.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="130"/>
        <source>Paste</source>
        <translation>Coller</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="132"/>
        <source>Replace the phrase with the one on the clipboard.</source>
        <translation>Remplacer la phrase par celle du presse-papiers.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="138"/>
        <source>Clear</source>
        <translation>Effacer</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="140"/>
        <source>Remove the phrase and fall back to the default book.</source>
        <translation>Supprimer la phrase et revenir au carnet par défaut.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="162"/>
        <source>A short code made from your phrase. Read it out with your friend to be sure you both have the same one. Unlike the phrase, this code is safe to say out loud.</source>
        <translation>Un code court fabriqué à partir de votre phrase. Lisez-le à voix haute avec votre ami pour vous assurer que vous avez tous les deux la même. Contrairement à la phrase, ce code peut être dit à voix haute sans danger.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="179"/>
        <source>Copy the fingerprint to the clipboard.</source>
        <translation>Copier l&apos;empreinte dans le presse-papiers.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="191"/>
        <source>The phrase is stored in the encrypted cache, never in the settings file. Send it to your friend over a private channel.</source>
        <translation>La phrase est stockée dans le cache chiffré, jamais dans le fichier de paramètres. Envoyez-la à votre ami par un canal privé.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="228"/>
        <source>Hide</source>
        <translation>Masquer</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="249"/>
        <source>No phrase set. Using the built-in default.</source>
        <translation>Aucune phrase définie. Utilisation de la valeur par défaut intégrée.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="250"/>
        <source>Phrase set. %1 characters.</source>
        <translation>Phrase définie. %1 caractères.</translation>
    </message>
    <message>
        <location filename="../ImSettingsPage.cpp" line="260"/>
        <source>Calculating…</source>
        <translation>Calcul en cours…</translation>
    </message>
</context>
</TS>
