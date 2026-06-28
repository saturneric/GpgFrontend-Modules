<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="fr_FR">
<context>
    <name>EMailMetaDataDialog</name>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="196"/>
        <source>The &apos;From&apos; field cannot be empty.</source>
        <translation>Le champ « De » ne peut pas être vide.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="198"/>
        <source>The &apos;From&apos; field must contain a valid email address.</source>
        <translation>Le champ « De » doit contenir une adresse e-mail valide.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="202"/>
        <source>The &apos;To&apos; field cannot be empty.</source>
        <translation>Le champ « À » ne peut pas être vide.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="206"/>
        <source>One or more &apos;To&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Une ou plusieurs adresses « À » sont invalides. Veuillez séparer les adresses multiples par un « ; ».</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="213"/>
        <source>One or more &apos;CC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Une ou plusieurs adresses « CC » sont invalides. Veuillez séparer les adresses multiples par un « ; ».</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="221"/>
        <source>One or more &apos;BCC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Une ou plusieurs adresses « CCI » sont invalides. Veuillez séparer les adresses multiples par un « ; ».</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="227"/>
        <source>The &apos;Subject&apos; field cannot be empty.</source>
        <translation>Le champ « Objet » ne peut pas être vide.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="20"/>
        <source>Message</source>
        <translation>Message</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="54"/>
        <source>From</source>
        <translation>De</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="77"/>
        <source>To</source>
        <translation>À</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="100"/>
        <location filename="../EMailMetaDataDialog.ui" line="207"/>
        <source>CC</source>
        <translation>CC</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="123"/>
        <location filename="../EMailMetaDataDialog.ui" line="214"/>
        <source>BCC</source>
        <translation>CCI</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="153"/>
        <source>Subject</source>
        <translation>Objet</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="175"/>
        <source>Tips: You can fill in multiple email addresses, please separate them with &quot;;&quot;, except for the &apos;From&apos; field.</source>
        <translation>Conseil : Vous pouvez saisir plusieurs adresses e-mail, veuillez les séparer par un « ; », sauf pour le champ « De ».</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="234"/>
        <source>Cancel</source>
        <translation>Annuler</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="241"/>
        <source>OK</source>
        <translation>OK</translation>
    </message>
</context>
<context>
    <name>EMailModule</name>
    <message>
        <location filename="../EMailModule.cpp" line="219"/>
        <source># EML Data Error

The provided EML data does not conform to RFC 3156 standards and cannot be processed.

**Details:** %1

### What is EML Data?
EML is a file format for representing email messages, typically including headers, body text, attachments, and metadata. Complete and properly structured EML data is required for validation.

### Suggested Solutions
1. Verify the EML data is complete and matches the structure outlined in RFC 3156.
2. Refer to the official documentation for the EML structure: %2

After correcting the EML data, try the operation again.</source>
        <translation># Erreur de données EML

Les données EML fournies ne sont pas conformes aux normes RFC 3156 et ne peuvent pas être traitées.

**Détails :** %1

### Qu&apos;est-ce que les données EML ?
EML est un format de fichier pour représenter des messages e-mail, comprenant généralement des en-têtes, le corps du texte, des pièces jointes et des métadonnées. Des données EML complètes et correctement structurées sont requises pour la validation.

### Solutions suggérées
1. Vérifiez que les données EML sont complètes et correspondent à la structure décrite dans la RFC 3156.
2. Consultez la documentation officielle sur la structure EML : %2

Après avoir corrigé les données EML, réessayez l&apos;opération.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="244"/>
        <source># Email Operation Error

An error occurred during the email operation. The process could not be completed.

**Details:**
- **Error Code:** %1
- **Error Message:** %2

### Possible Causes
1. The email data may be incomplete or corrupted.
2. The selected GPG key does not have the necessary permissions.
3. Issues in the GPG environment or configuration.

### Suggested Solutions
1. Ensure the email data is complete and follows the expected format.
2. Verify the GPG key has the required access permissions.
3. Check your GPG environment and configuration settings.
4. Review the error details above or application logs for further troubleshooting.

If the issue persists, consider seeking technical support or consulting the documentation.</source>
        <translation># Erreur d&apos;opération e-mail

Une erreur s&apos;est produite lors de l&apos;opération e-mail. Le processus n&apos;a pas pu être terminé.

**Détails :**
- **Code d&apos;erreur :** %1
- **Message d&apos;erreur :** %2

### Causes possibles
1. Les données e-mail peuvent être incomplètes ou corrompues.
2. La clé GPG sélectionnée ne dispose pas des autorisations nécessaires.
3. Problèmes dans l&apos;environnement ou la configuration GPG.

### Solutions suggérées
1. Assurez-vous que les données e-mail sont complètes et respectent le format attendu.
2. Vérifiez que la clé GPG dispose des autorisations d&apos;accès requises.
3. Vérifiez votre environnement et vos paramètres de configuration GPG.
4. Consultez les détails de l&apos;erreur ci-dessus ou les journaux de l&apos;application pour un dépannage supplémentaire.

Si le problème persiste, envisagez de contacter le support technique ou de consulter la documentation.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="93"/>
        <location filename="../EMailModule.cpp" line="415"/>
        <location filename="../EMailModule.cpp" line="546"/>
        <location filename="../EMailModule.cpp" line="1230"/>
        <source>From</source>
        <translation>De</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="92"/>
        <source>E-Mail</source>
        <translation>E-mail</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="94"/>
        <location filename="../EMailModule.cpp" line="418"/>
        <location filename="../EMailModule.cpp" line="549"/>
        <location filename="../EMailModule.cpp" line="1233"/>
        <source>To</source>
        <translation>À</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="95"/>
        <location filename="../EMailModule.cpp" line="422"/>
        <location filename="../EMailModule.cpp" line="553"/>
        <location filename="../EMailModule.cpp" line="1237"/>
        <source>Subject</source>
        <translation>Objet</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="96"/>
        <location filename="../EMailModule.cpp" line="425"/>
        <location filename="../EMailModule.cpp" line="556"/>
        <location filename="../EMailModule.cpp" line="1240"/>
        <source>CC</source>
        <translation>Cc</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="97"/>
        <location filename="../EMailModule.cpp" line="428"/>
        <location filename="../EMailModule.cpp" line="559"/>
        <location filename="../EMailModule.cpp" line="1243"/>
        <source>BCC</source>
        <translation>Cci</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="98"/>
        <location filename="../EMailModule.cpp" line="431"/>
        <location filename="../EMailModule.cpp" line="562"/>
        <location filename="../EMailModule.cpp" line="1246"/>
        <source>Date</source>
        <translation>Date</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="105"/>
        <source>OpenPGP</source>
        <translation>OpenPGP</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="106"/>
        <location filename="../EMailModule.cpp" line="439"/>
        <location filename="../EMailModule.cpp" line="1254"/>
        <source>Signed EML Data Hash (SHA1)</source>
        <translation>Hachage des données EML signées (SHA1)</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="108"/>
        <location filename="../EMailModule.cpp" line="444"/>
        <location filename="../EMailModule.cpp" line="1259"/>
        <source>Message Integrity Check Algorithm</source>
        <translation>Algorithme de contrôle d&apos;intégrité du message</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="136"/>
        <source>Encryption Recipient</source>
        <translation>Destinataire du chiffrement</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="137"/>
        <source>Recipient</source>
        <translation>Destinataire</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="138"/>
        <source>Key ID</source>
        <translation>Identifiant de clé</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="453"/>
        <source>Verify E-Mail</source>
        <translation>Vérifier l&apos;e-mail</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="571"/>
        <source>Decrypt E-Mail</source>
        <translation>Déchiffrer l&apos;e-mail</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="748"/>
        <location filename="../EMailModule.cpp" line="773"/>
        <source>Sign E-Mail</source>
        <translation>Signer l&apos;e-mail</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="933"/>
        <location filename="../EMailModule.cpp" line="970"/>
        <source>Encrypt E-Mail</source>
        <translation>Chiffrer l&apos;e-mail</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1103"/>
        <location filename="../EMailModule.cpp" line="1150"/>
        <source>Encrypt and Sign E-Mail</source>
        <translation>Chiffrer et signer l&apos;e-mail</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1268"/>
        <source>Decrypt and Verify E-Mail</source>
        <translation>Déchiffrer et vérifier l&apos;e-mail</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1316"/>
        <source>Save file</source>
        <translation>Enregistrer le fichier</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1343"/>
        <location filename="../EMailModule.cpp" line="1388"/>
        <location filename="../EMailModule.cpp" line="1415"/>
        <source>Warning</source>
        <translation>Avertissement</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1344"/>
        <source>Cannot read file%1:
%2.</source>
        <translation>Impossible de lire le fichier%1 :
%2.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1389"/>
        <source>The file %1 is too large (%2 bytes) to be opened. The maximum allowed size is 1 MB.</source>
        <translation>Le fichier %1 est trop volumineux (%2 octets) pour être ouvert. La taille maximale autorisée est de 1 Mo.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1416"/>
        <source>Cannot read file %1:
%2.</source>
        <translation>Impossible de lire le fichier %1 :
%2.</translation>
    </message>
</context>
<context>
    <name>GTrC</name>
    <message>
        <location filename="../EMailModule.cpp" line="320"/>
        <source>Mail Editor</source>
        <translation>Éditeur de courriel</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="322"/>
        <source>Open a new text editor for email.</source>
        <translation>Ouvrir un nouvel éditeur de texte pour courriel.</translation>
    </message>
</context>
</TS>
