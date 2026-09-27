<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de_DE">
<context>
    <name>KeyServerSettingsPage</name>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="17"/>
        <location filename="../KeyServerSettingsPage.cpp" line="57"/>
        <source>Key Server List</source>
        <translation>Schlüsselserver-Liste</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="49"/>
        <location filename="../KeyServerSettingsPage.cpp" line="58"/>
        <source>Add a Key Server</source>
        <translation>Schlüsselserver hinzufügen</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="82"/>
        <location filename="../KeyServerSettingsPage.cpp" line="59"/>
        <source>Operations</source>
        <translation>Vorgänge</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="60"/>
        <location filename="../KeyServerSettingsPage.cpp" line="61"/>
        <source>Add</source>
        <translation>Hinzufügen</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="88"/>
        <location filename="../KeyServerSettingsPage.cpp" line="62"/>
        <source>Set As Default</source>
        <translation>Als Standard festlegen</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="95"/>
        <location filename="../KeyServerSettingsPage.cpp" line="63"/>
        <source>Test Selected</source>
        <translation>Ausgewählten testen</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="102"/>
        <location filename="../KeyServerSettingsPage.cpp" line="64"/>
        <source>Delete Selected</source>
        <translation>Ausgewählte löschen</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="66"/>
        <source>https://keys.example.org</source>
        <translation>https://keys.example.org</translation>
    </message>
    <message>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP; publishing and refreshing use VKS.</source>
        <translation type="vanished">Ein neuer Schlüsselserver wird vor dem Hinzufügen gegen die HKP- und VKS-Schnittstellen getestet. Die Suche verwendet HKP; Veröffentlichung und Aktualisierung verwenden VKS.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="67"/>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP. Publishing and refreshing always use the default server: over VKS where it offers it, over HKP otherwise.</source>
        <translation>Ein neuer Schlüsselserver wird vor dem Hinzufügen gegen die HKP- und VKS-Schnittstellen getestet. Die Suche verwendet HKP. Veröffentlichung und Aktualisierung verwenden immer den Standardserver: über VKS, sofern der Server dies unterstützt, andernfalls über HKP.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="74"/>
        <source>Default</source>
        <translation>Standard</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="74"/>
        <source>Address</source>
        <translation>Adresse</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="74"/>
        <source>HKP</source>
        <translation>HKP</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="74"/>
        <source>VKS</source>
        <translation>VKS</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="74"/>
        <source>Status</source>
        <translation>Status</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Last Tested</source>
        <translation>Zuletzt getestet</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="108"/>
        <source>yes</source>
        <translation>Ja</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="109"/>
        <source>no</source>
        <translation>Nein</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Verified</source>
        <translation>Verifiziert</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Unverified</source>
        <translation>Nicht verifiziert</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="136"/>
        <source>never</source>
        <translation>Nie</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="162"/>
        <source>Invalid Address</source>
        <translation>Ungültige Adresse</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="163"/>
        <source>&quot;%1&quot; is not a valid key server address.</source>
        <translation>&quot;%1&quot; ist keine gültige Schlüsselserver-Adresse.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="173"/>
        <source>Already Listed</source>
        <translation>Bereits aufgelistet</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="174"/>
        <source>%1 is already in the key server list.</source>
        <translation>%1 ist bereits in der Schlüsselserver-Liste.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="180"/>
        <source>Insecure Key Server Address</source>
        <translation>Unsicherer Schlüsselserver-Adresse</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="181"/>
        <source>%1 uses plain HTTP, so anyone on the network can see and change what you look up or publish. Add it anyway?</source>
        <translation>%1 verwendet unverschlüsseltes HTTP, sodass jeder im Netzwerk sehen und ändern kann, was Sie nachschlagen oder veröffentlichen. Trotzdem hinzufügen?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="229"/>
        <source>No Verified Publishing</source>
        <translation>Keine verifizierte Veröffentlichung</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="230"/>
        <source>%1 does not support the VKS interface, so publishing and refreshing will use HKP instead.

Over HKP the server does not confirm your email address, and an uploaded key cannot be removed again.

Use %1 as the default anyway?</source>
        <translation>%1 unterstützt die VKS-Schnittstelle nicht, daher verwenden Veröffentlichung und Aktualisierung stattdessen HKP.

Über HKP bestätigt der Server Ihre E-Mail-Adresse nicht, und ein hochgeladener Schlüssel kann nicht wieder entfernt werden.

%1 trotzdem als Standard verwenden?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="291"/>
        <source>Key Server Not Verified</source>
        <translation>Schlüsselserver nicht verifiziert</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="292"/>
        <source>%1 did not answer as a key server.

%2

It has been added and marked unverified; use Test Selected to try again.</source>
        <translation>%1 hat nicht als Schlüsselserver geantwortet.

%2

Es wurde hinzugefügt und als unverifiziert markiert; verwenden Sie &quot;Ausgewählten testen&quot;, um es erneut zu versuchen.</translation>
    </message>
</context>
<context>
    <name>ModuleKeyServerSync</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="416"/>
        <source>The following email addresses have status:
</source>
        <translation>Die folgenden E-Mail-Adressen haben folgenden Status:</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="425"/>
        <source>Could not parse status information.</source>
        <translation>Statusinformationen konnten nicht analysiert werden.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="396"/>
        <location filename="../KeyServerSyncModule.cpp" line="433"/>
        <source>Public Key Upload Successful</source>
        <translation>Öffentlicher Schlüssel erfolgreich hochgeladen</translation>
    </message>
    <message>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation type="vanished">Der öffentliche Schlüssel wurde erfolgreich auf dem Schlüsselserver keys.openpgp.org hochgeladen.
Fingerabdruck: %1

%2
Bitte prüfen Sie Ihre E-Mail (%3) auf weitere Bestätigungen von keys.openpgp.org.

Hinweis: Weitere Informationen zur Verifizierung finden Sie hier: https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="365"/>
        <location filename="../KeyServerSyncModule.cpp" line="387"/>
        <location filename="../KeyServerSyncModule.cpp" line="446"/>
        <source>Key Upload Failed</source>
        <translation>Hochladen des Schlüssels fehlgeschlagen</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="366"/>
        <source>Failed to export the public key before uploading.
Key: %1</source>
        <translation>Der öffentliche Schlüssel konnte vor dem Hochladen nicht exportiert werden.
Schlüssel: %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="434"/>
        <source>The public key was successfully uploaded to the key server %4.
Fingerprint: %1

%2
Please check your email (%3) for further verification from %4.</source>
        <translation>Der öffentliche Schlüssel wurde erfolgreich auf dem Schlüsselserver %4 hochgeladen.
Fingerabdruck: %1

%2
Bitte prüfen Sie Ihre E-Mail (%3) auf weitere Bestätigungen von %4.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="388"/>
        <location filename="../KeyServerSyncModule.cpp" line="447"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>Der öffentliche Schlüssel konnte nicht auf den Server hochgeladen werden.
Fingerabdruck: %1
Fehler: %2</translation>
    </message>
    <message>
        <source>The key server did not return a key.</source>
        <translation type="vanished">Der Schlüsselserver hat keinen Schlüssel zurückgegeben.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="155"/>
        <source>Publish Without Verification?</source>
        <translation>Ohne Verifizierung veröffentlichen?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="156"/>
        <source>%1 does not support verified publishing (VKS), so the key would be uploaded over HKP instead.

The server will not confirm your email address, and the upload cannot be undone: HKP key servers do not allow keys to be removed.

Publish to %1 anyway?</source>
        <translation>%1 unterstützt keine verifizierte Veröffentlichung (VKS), daher würde der Schlüssel stattdessen über HKP hochgeladen werden.

Der Server wird Ihre E-Mail-Adresse nicht bestätigen, und der Upload kann nicht rückgängig gemacht werden – HKP-Schlüsselserver erlauben keine Entfernung von Schlüsseln.

Trotzdem auf %1 veröffentlichen?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="230"/>
        <source>%1: the public key could not be exported</source>
        <translation>%1: Der öffentliche Schlüssel konnte nicht exportiert werden.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="278"/>
        <source>Key Refresh Finished</source>
        <translation>Schlüsselaktualisierung abgeschlossen</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="282"/>
        <source>Key Publishing Finished</source>
        <translation>Schlüsselveröffentlichung abgeschlossen</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="304"/>
        <source>A key server operation is already running. Try again when it has finished.</source>
        <translation>Es läuft bereits ein Schlüsselserver-Vorgang. Versuchen Sie es erneut, wenn er abgeschlossen ist.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="349"/>
        <source>Key Servers</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="350"/>
        <source>keyserver,key server,hkp,vks,publish,search</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="397"/>
        <source>The public key was uploaded to the key server %2 over HKP.
Fingerprint: %1</source>
        <translation>Der öffentliche Schlüssel wurde über HKP auf den Schlüsselserver %2 hochgeladen.
Fingerabdruck: %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="476"/>
        <source>Key Update Failed</source>
        <translation>Schlüsselaktualisierung fehlgeschlagen</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="477"/>
        <source>Failed to retrieve public key from %3.
Key ID: %1
Error: %2</source>
        <translation>Der öffentliche Schlüssel konnte nicht von %3 abgerufen werden.
Schlüssel-ID: %1
Fehler: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="520"/>
        <source>Upload the public key to the key server used for syncing</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="532"/>
        <source>Import the latest copy of the public key from the key server</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="544"/>
        <source>Check Publication Status</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="545"/>
        <source>Ask the key server whether it has this public key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="614"/>
        <source>Publication Status</source>
        <translation>Veröffentlichungsstatus</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="619"/>
        <source>The public key has been published on %1.</source>
        <translation>Der öffentliche Schlüssel wurde auf %1 veröffentlicht.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="624"/>
        <source>The public key is not published on %1.</source>
        <translation>Der öffentliche Schlüssel ist nicht auf %1 veröffentlicht.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="628"/>
        <source>Could not ask %1 about this key.

%2</source>
        <translation>Bei %1 konnte keine Auskunft zu diesem Schlüssel eingeholt werden.

%2</translation>
    </message>
    <message>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation type="vanished">Der öffentliche Schlüssel konnte nicht vom Server abgerufen werden.
Schlüssel-ID: %1
Fehler: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="303"/>
        <location filename="../KeyServerSyncModule.cpp" line="334"/>
        <location filename="../KeyServerSyncModule.cpp" line="556"/>
        <source>Key Server</source>
        <translation>Schlüsselserver</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="557"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>Öffentliche Schlüssel von einem vertrauenswürdigen Schlüsselserver importieren.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="521"/>
        <location filename="../KeyServerSyncModule.cpp" line="534"/>
        <location filename="../KeyServerSyncModule.cpp" line="546"/>
        <source>Key Server Operations</source>
        <translation>Schlüsselserver-Vorgänge</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="519"/>
        <source>Publish Public Key to Key Server</source>
        <translation>Öffentlichen Schlüssel auf Schlüsselserver veröffentlichen</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="531"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>Öffentlichen Schlüssel vom Schlüsselserver aktualisieren</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="246"/>
        <source>The server could not be reached.</source>
        <translation>Der Server konnte nicht erreicht werden.</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="247"/>
        <source>The server responded, but not as a key server: it supports neither the HKP nor the VKS interface.</source>
        <translation>Der Server hat geantwortet, aber nicht als Schlüsselserver: Er unterstützt weder das HKP- noch das VKS-Interface.</translation>
    </message>
    <message>
        <location filename="../KeyServerBatchLogic.cpp" line="44"/>
        <source>Failed:</source>
        <translation>Fehlgeschlagen:</translation>
    </message>
    <message>
        <location filename="../KeyServerBatchLogic.cpp" line="79"/>
        <source>The key server does not have this key.</source>
        <translation>Der Schlüsselserver hat diesen Schlüssel nicht.</translation>
    </message>
    <message>
        <location filename="../KeyServerBatchLogic.cpp" line="88"/>
        <source>%1 of %2 keys were fetched from the key server.</source>
        <translation>%1 von %2 Schlüsseln wurden vom Schlüsselserver abgerufen.</translation>
    </message>
    <message>
        <location filename="../KeyServerBatchLogic.cpp" line="97"/>
        <source>%1 of %2 keys were published to the key server %3.</source>
        <translation>%1 von %2 Schlüsseln wurden auf den Schlüsselserver %3 veröffentlicht.</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Key ID</source>
        <translation>Schlüssel-ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Creation Date</source>
        <translation>Erstellungsdatum</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Expiration Date</source>
        <translation>Ablaufdatum</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Algorithm</source>
        <translation>Algorithmus</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Key Size</source>
        <translation>Schlüsselgröße</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="112"/>
        <source>Status</source>
        <translation>Status</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="115"/>
        <source>By Key ID</source>
        <translation>Nach Schlüssel-ID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="116"/>
        <source>By Email</source>
        <translation>Nach E-Mail</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="117"/>
        <source>By Fingerprint</source>
        <translation>Nach Fingerabdruck</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="120"/>
        <source>Enter a value, then press Enter or Search</source>
        <translation>Geben Sie einen Wert ein und drücken Sie dann Enter oder Suchen.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="163"/>
        <source>Search value is empty.</source>
        <translation>Der Suchwert ist leer.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Key server URL is empty.</source>
        <translation>Die Schlüsselserver-URL ist leer.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="188"/>
        <source>Invalid key server URL format.</source>
        <translation>Ungültiges Format der Schlüsselserver-URL.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="201"/>
        <source>Invalid email format.</source>
        <translation>Ungültiges E-Mail-Format.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="216"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16, 40 or 64.</source>
        <translation>Ungültiges Format des Fingerabdrucks. Es sollte ein Hex-String der Länge 16, 40 oder 64 sein.</translation>
    </message>
    <message>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation type="vanished">Ungültiges Format des Fingerabdrucks. Es sollte ein Hex-String der Länge 16 oder 40 sein.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="230"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>Ungültiges Format der Schlüssel-ID. Es sollte ein Hex-String der Länge 8 oder 16 sein.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="236"/>
        <source>Unknown search type.</source>
        <translation>Unbekannter Suchtyp.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="273"/>
        <location filename="../SearchKeyDialog.cpp" line="283"/>
        <source>No keys found matching your search.</source>
        <translation>Keine Schlüssel gefunden, die Ihrer Suche entsprechen.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="302"/>
        <source>(no user ID published)</source>
        <translation>(keine Benutzer-ID veröffentlicht)</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="306"/>
        <source>Unknown</source>
        <translation>Unbekannt</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="310"/>
        <source>Never</source>
        <translation>Nie</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="374"/>
        <source>No GPG context is available.</source>
        <translation>Kein GPG-Kontext verfügbar.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="14"/>
        <source>Search Keys</source>
        <translation>Schlüssel suchen</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="36"/>
        <source>Key Server</source>
        <translation>Schlüsselserver</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="56"/>
        <source>Search Type</source>
        <translation>Suchtyp</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="66"/>
        <source>Search Value</source>
        <translation>Suchwert</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="85"/>
        <source>Search</source>
        <translation>Suchen</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="105"/>
        <source>Tips: double click to import the selected key.</source>
        <translation>Tipp: Doppelklicken Sie zum Importieren des ausgewählten Schlüssels.</translation>
    </message>
</context>
</TS>
