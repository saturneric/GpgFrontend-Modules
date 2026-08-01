<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="it_IT">
<context>
    <name>GTrC</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="119"/>
        <source>The following email addresses have status:
</source>
        <translation>I seguenti indirizzi email hanno lo stato:</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="129"/>
        <source>Could not parse status information.</source>
        <translation>Impossibile analizzare le informazioni sullo stato.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="140"/>
        <source>Public Key Upload Successful</source>
        <translation>Caricamento della chiave pubblica riuscito</translation>
    </message>
    <message>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation type="vanished">La chiave pubblica è stata caricata con successo sul server delle chiavi keys.openpgp.org.
Impronta digitale: %1

%2
Controlla la tua email (%3) per ulteriori verifiche da keys.openpgp.org.

Nota: Per la verifica, puoi trovare maggiori informazioni qui: https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="99"/>
        <location filename="../KeyServerSyncModule.cpp" line="156"/>
        <source>Key Upload Failed</source>
        <translation>Caricamento della chiave pubblica fallito</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="100"/>
        <source>Failed to export the public key before uploading.
Key: %1</source>
        <translation>Impossibile esportare la chiave pubblica prima del caricamento.
Chiave: %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="141"/>
        <source>The public key was successfully uploaded to the key server %4.
Fingerprint: %1

%2
Please check your email (%3) for further verification from %4.</source>
        <translation>La chiave pubblica è stata caricata con successo sul server delle chiavi %4.
Impronta digitale: %1

%2
Controlla la tua email (%3) per ulteriori verifiche da %4.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="157"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>Impossibile caricare la chiave pubblica sul server.
Impronta digitale: %1
Errore: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="191"/>
        <source>Key Update Failed</source>
        <translation>Aggiornamento della chiave fallito</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="192"/>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation>Impossibile recuperare la chiave pubblica dal server.
ID chiave: %1
Errore: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="244"/>
        <source>Key Server</source>
        <translation>Server delle chiavi</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="245"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>Importa chiavi pubbliche da un server delle chiavi attendibile.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="293"/>
        <source>Key Server Operations</source>
        <translation>Operazioni del server delle chiavi</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="298"/>
        <source>Publish Public Key to Key Server</source>
        <translation>Pubblica chiave pubblica sul server delle chiavi</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="305"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>Aggiorna chiave pubblica dal server delle chiavi</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="247"/>
        <source>The server could not be reached.</source>
        <translation>Impossibile raggiungere il server.</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="249"/>
        <source>The server responded, but not as a key server: it supports neither the HKP nor the VKS interface.</source>
        <translation>Il server ha risposto, ma non come server delle chiavi: non supporta né l&apos;interfaccia HKP né VKS.</translation>
    </message>
</context>
<context>
    <name>KeyServerSettingsPage</name>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="17"/>
        <location filename="../KeyServerSettingsPage.cpp" line="58"/>
        <source>Key Server List</source>
        <translation>Elenco dei server di chiavi</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="49"/>
        <location filename="../KeyServerSettingsPage.cpp" line="59"/>
        <source>Add a Key Server</source>
        <translation>Aggiungi un server di chiavi</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="82"/>
        <location filename="../KeyServerSettingsPage.cpp" line="60"/>
        <source>Operations</source>
        <translation>Operazioni</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="60"/>
        <location filename="../KeyServerSettingsPage.cpp" line="62"/>
        <source>Add</source>
        <translation>Aggiungi</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="88"/>
        <location filename="../KeyServerSettingsPage.cpp" line="63"/>
        <source>Set As Default</source>
        <translation>Imposta come predefinito</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="95"/>
        <location filename="../KeyServerSettingsPage.cpp" line="64"/>
        <source>Test Selected</source>
        <translation>Testa selezionato</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="102"/>
        <location filename="../KeyServerSettingsPage.cpp" line="65"/>
        <source>Delete Selected</source>
        <translation>Elimina selezionato</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="67"/>
        <source>https://keys.example.org</source>
        <translation>https://keys.example.org</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="68"/>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP; publishing and refreshing use VKS.</source>
        <translation>Un nuovo server di chiavi viene testato rispetto alle interfacce HKP e VKS prima di essere aggiunto. La ricerca utilizza HKP; la pubblicazione e l&apos;aggiornamento utilizzano VKS.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="74"/>
        <source>Default</source>
        <translation>Predefinito</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="74"/>
        <source>Address</source>
        <translation>Indirizzo</translation>
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
        <translation>Stato</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Last Tested</source>
        <translation>Ultimo test</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="107"/>
        <source>yes</source>
        <translation>Sì</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="108"/>
        <source>no</source>
        <translation>No</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="126"/>
        <source>Verified</source>
        <translation>Verificato</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="126"/>
        <source>Unverified</source>
        <translation>Non verificato</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="135"/>
        <source>never</source>
        <translation>mai</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="161"/>
        <source>Invalid Address</source>
        <translation>Indirizzo non valido</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="162"/>
        <source>&quot;%1&quot; is not a valid key server address.</source>
        <translation>&quot;%1&quot; non è un indirizzo del server di chiavi valido.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="172"/>
        <source>Already Listed</source>
        <translation>Già elencato</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="173"/>
        <source>%1 is already in the key server list.</source>
        <translation>%1 è già presente nella lista dei server di chiavi.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="179"/>
        <source>Insecure Key Server Address</source>
        <translation>Indirizzo del Server di Chiavi Non Sicuro</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="180"/>
        <source>%1 uses plain HTTP, so anyone on the network can see and change what you look up or publish. Add it anyway?</source>
        <translation>%1 utilizza HTTP semplice, quindi chiunque sulla rete può vedere e modificare ciò che cerchi o pubblichi. Aggiungerlo comunque?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="272"/>
        <source>Key Server Not Verified</source>
        <translation>Server di Chiavi Non Verificato</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="273"/>
        <source>%1 did not answer as a key server.

%2

It has been added and marked unverified; use Test Selected to try again.</source>
        <translation>%1 non ha risposto come server di chiavi.

%2

È stato aggiunto e contrassegnato come non verificato; usa &quot;Test Selezionato&quot; per riprovare.</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="75"/>
        <source>Key ID</source>
        <translation>ID chiave</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="75"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="75"/>
        <source>Creation Date</source>
        <translation>Data di creazione</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="76"/>
        <source>Expiration Date</source>
        <translation>Data di scadenza</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="76"/>
        <source>Algorithm</source>
        <translation>Algoritmo</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="76"/>
        <source>Key Size</source>
        <translation>Dimensione chiave</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="77"/>
        <source>Status</source>
        <translation>Stato</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="80"/>
        <source>By Key ID</source>
        <translation>Per ID chiave</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="81"/>
        <source>By Email</source>
        <translation>Per email</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="82"/>
        <source>By Fingerprint</source>
        <translation>Per impronta digitale</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="85"/>
        <source>Enter a value, then press Enter or Search</source>
        <translation>Inserisci un valore, poi premi Invio o Cerca</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="128"/>
        <source>Search value is empty.</source>
        <translation>Il valore di ricerca è vuoto.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="144"/>
        <source>Key server URL is empty.</source>
        <translation>L&apos;URL del server delle chiavi è vuoto.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="153"/>
        <source>Invalid key server URL format.</source>
        <translation>Formato dell&apos;URL del server delle chiavi non valido.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="166"/>
        <source>Invalid email format.</source>
        <translation>Formato dell&apos;email non valido.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation>Formato dell&apos;impronta digitale non valido. Deve essere una stringa esadecimale di lunghezza 16 o 40.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="193"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>Formato dell&apos;ID chiave non valido. Deve essere una stringa esadecimale di lunghezza 8 o 16.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="199"/>
        <source>Unknown search type.</source>
        <translation>Tipo di ricerca sconosciuto.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="236"/>
        <location filename="../SearchKeyDialog.cpp" line="246"/>
        <source>No keys found matching your search.</source>
        <translation>Nessuna chiave trovata corrispondente alla ricerca.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="331"/>
        <source>No GPG context is available.</source>
        <translation>Contesto GPG non disponibile.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="14"/>
        <source>Search Keys</source>
        <translation>Cerca chiavi</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="36"/>
        <source>Key Server</source>
        <translation>Server delle chiavi</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="56"/>
        <source>Search Type</source>
        <translation>Tipo di ricerca</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="66"/>
        <source>Search Value</source>
        <translation>Valore di ricerca</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="85"/>
        <source>Search</source>
        <translation>Cerca</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="105"/>
        <source>Tips: double click to import the selected key.</source>
        <translation>Suggerimento: fai doppio clic per importare la chiave selezionata.</translation>
    </message>
</context>
</TS>
