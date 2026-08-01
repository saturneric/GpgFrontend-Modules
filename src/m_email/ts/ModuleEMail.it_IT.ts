<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="it_IT">
<context>
    <name>EMailMetaDataDialog</name>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="196"/>
        <source>The &apos;From&apos; field cannot be empty.</source>
        <translation>Il campo &apos;Da&apos; non può essere vuoto.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="198"/>
        <source>The &apos;From&apos; field must contain a valid email address.</source>
        <translation>Il campo &apos;Da&apos; deve contenere un indirizzo email valido.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="202"/>
        <source>The &apos;To&apos; field cannot be empty.</source>
        <translation>Il campo &apos;A&apos; non può essere vuoto.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="206"/>
        <source>One or more &apos;To&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Uno o più indirizzi &apos;A&apos; non sono validi. Separare più indirizzi con &quot;;&quot;.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="213"/>
        <source>One or more &apos;CC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Uno o più indirizzi &apos;CC&apos; non sono validi. Separare più indirizzi con &quot;;&quot;.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="221"/>
        <source>One or more &apos;BCC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Uno o più indirizzi &apos;BCC&apos; non sono validi. Separare più indirizzi con &quot;;&quot;.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="227"/>
        <source>The &apos;Subject&apos; field cannot be empty.</source>
        <translation>Il campo &apos;Oggetto&apos; non può essere vuoto.</translation>
    </message>
    <message>
        <source>Message</source>
        <translation type="vanished">Messaggio</translation>
    </message>
    <message>
        <source>From</source>
        <translation type="vanished">Da</translation>
    </message>
    <message>
        <source>To</source>
        <translation type="vanished">A</translation>
    </message>
    <message>
        <source>CC</source>
        <translation type="vanished">CC</translation>
    </message>
    <message>
        <source>BCC</source>
        <translation type="vanished">BCC</translation>
    </message>
    <message>
        <source>Subject</source>
        <translation type="vanished">Oggetto</translation>
    </message>
    <message>
        <source>Tips: You can fill in multiple email addresses, please separate them with &quot;;&quot;, except for the &apos;From&apos; field.</source>
        <translation type="vanished">Suggerimento: è possibile inserire più indirizzi email, separandoli con &quot;;&quot;, ad eccezione del campo &apos;Da&apos;.</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation type="vanished">Annulla</translation>
    </message>
    <message>
        <source>OK</source>
        <translation type="vanished">OK</translation>
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
        <translation># Errore dati EML

I dati EML forniti non sono conformi agli standard RFC 3156 e non possono essere elaborati.

**Dettagli:** %1

### Cosa sono i dati EML?
EML è un formato file per la rappresentazione di messaggi email, che include tipicamente intestazioni, corpo del testo, allegati e metadati. Per la convalida sono richiesti dati EML completi e correttamente strutturati.

### Soluzioni suggerite
1. Verificare che i dati EML siano completi e corrispondano alla struttura descritta nella RFC 3156.
2. Fare riferimento alla documentazione ufficiale per la struttura EML: %2

Dopo aver corretto i dati EML, riprovare l&apos;operazione.</translation>
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
        <translation># Errore operazione email

Si è verificato un errore durante l&apos;operazione email. Il processo non è stato completato.

**Dettagli:**
- **Codice errore:** %1
- **Messaggio di errore:** %2

### Possibili cause
1. I dati email potrebbero essere incompleti o corrotti.
2. La chiave GPG selezionata non dispone delle autorizzazioni necessarie.
3. Problemi nell&apos;ambiente o nella configurazione GPG.

### Soluzioni suggerite
1. Assicurarsi che i dati email siano completi e seguano il formato previsto.
2. Verificare che la chiave GPG disponga delle autorizzazioni di accesso richieste.
3. Controllare l&apos;ambiente e le impostazioni di configurazione GPG.
4. Esaminare i dettagli dell&apos;errore sopra o i log dell&apos;applicazione per ulteriori diagnosi.

Se il problema persiste, si consiglia di contattare il supporto tecnico o consultare la documentazione.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="93"/>
        <location filename="../EMailModule.cpp" line="416"/>
        <location filename="../EMailModule.cpp" line="547"/>
        <location filename="../EMailModule.cpp" line="1230"/>
        <source>From</source>
        <translation>Da</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="92"/>
        <source>E-Mail</source>
        <translation>Email</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="94"/>
        <location filename="../EMailModule.cpp" line="419"/>
        <location filename="../EMailModule.cpp" line="550"/>
        <location filename="../EMailModule.cpp" line="1233"/>
        <source>To</source>
        <translation>A</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="95"/>
        <location filename="../EMailModule.cpp" line="423"/>
        <location filename="../EMailModule.cpp" line="554"/>
        <location filename="../EMailModule.cpp" line="1237"/>
        <source>Subject</source>
        <translation>Oggetto</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="96"/>
        <location filename="../EMailModule.cpp" line="426"/>
        <location filename="../EMailModule.cpp" line="557"/>
        <location filename="../EMailModule.cpp" line="1240"/>
        <source>CC</source>
        <translation>CC</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="97"/>
        <location filename="../EMailModule.cpp" line="429"/>
        <location filename="../EMailModule.cpp" line="560"/>
        <location filename="../EMailModule.cpp" line="1243"/>
        <source>BCC</source>
        <translation>BCC</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="98"/>
        <location filename="../EMailModule.cpp" line="432"/>
        <location filename="../EMailModule.cpp" line="563"/>
        <location filename="../EMailModule.cpp" line="1246"/>
        <source>Date</source>
        <translation>Data</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="105"/>
        <source>OpenPGP</source>
        <translation>OpenPGP</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="106"/>
        <location filename="../EMailModule.cpp" line="440"/>
        <location filename="../EMailModule.cpp" line="1254"/>
        <source>Signed EML Data Hash (SHA1)</source>
        <translation>Hash dei dati EML firmati (SHA1)</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="108"/>
        <location filename="../EMailModule.cpp" line="445"/>
        <location filename="../EMailModule.cpp" line="1259"/>
        <source>Message Integrity Check Algorithm</source>
        <translation>Algoritmo di controllo integrità messaggio</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="136"/>
        <source>Encryption Recipient</source>
        <translation>Destinatario cifratura</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="137"/>
        <source>Recipient</source>
        <translation>Destinatario</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="138"/>
        <source>Key ID</source>
        <translation>ID chiave</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="454"/>
        <source>Verify E-Mail</source>
        <translation>Verifica Email</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="572"/>
        <source>Decrypt E-Mail</source>
        <translation>Decifra Email</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="749"/>
        <location filename="../EMailModule.cpp" line="774"/>
        <source>Sign E-Mail</source>
        <translation>Firma Email</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="933"/>
        <location filename="../EMailModule.cpp" line="970"/>
        <source>Encrypt E-Mail</source>
        <translation>Cifra Email</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1103"/>
        <location filename="../EMailModule.cpp" line="1150"/>
        <source>Encrypt and Sign E-Mail</source>
        <translation>Cifra e firma Email</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1268"/>
        <source>Decrypt and Verify E-Mail</source>
        <translation>Decifra e verifica Email</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1316"/>
        <source>Save file</source>
        <translation>Salva file</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1343"/>
        <location filename="../EMailModule.cpp" line="1388"/>
        <location filename="../EMailModule.cpp" line="1415"/>
        <source>Warning</source>
        <translation>Avviso</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1344"/>
        <source>Cannot read file%1:
%2.</source>
        <translation>Impossibile leggere il file%1:
%2.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1389"/>
        <source>The file %1 is too large (%2 bytes) to be opened. The maximum allowed size is 1 MB.</source>
        <translation>Il file %1 è troppo grande (%2 byte) per essere aperto. La dimensione massima consentita è 1 MB.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1416"/>
        <source>Cannot read file %1:
%2.</source>
        <translation>Impossibile leggere il file %1:
%2.</translation>
    </message>
</context>
<context>
    <name>GTrC</name>
    <message>
        <location filename="../EMailModule.cpp" line="320"/>
        <source>Mail Editor</source>
        <translation>Editor di posta</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="322"/>
        <source>Open a new text editor for email.</source>
        <translation>Apri un nuovo editor di testo per email.</translation>
    </message>
</context>
</TS>
