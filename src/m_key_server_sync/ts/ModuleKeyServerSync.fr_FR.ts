<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="fr_FR">
<context>
    <name>GTrC</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="252"/>
        <source>The following email addresses have status:
</source>
        <translation>Les adresses e-mail suivantes ont le statut suivant :</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="262"/>
        <source>Could not parse status information.</source>
        <translation>Impossible d&apos;analyser les informations de statut.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="230"/>
        <location filename="../KeyServerSyncModule.cpp" line="273"/>
        <source>Public Key Upload Successful</source>
        <translation>Téléversement de la clé publique réussi</translation>
    </message>
    <message>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation type="vanished">La clé publique a été téléversée avec succès sur le serveur de clés keys.openpgp.org.
Empreinte : %1

%2
Veuillez vérifier votre e-mail (%3) pour la vérification supplémentaire de keys.openpgp.org.

Remarque : Pour la vérification, vous pouvez trouver plus d&apos;informations ici : https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="189"/>
        <location filename="../KeyServerSyncModule.cpp" line="217"/>
        <location filename="../KeyServerSyncModule.cpp" line="289"/>
        <source>Key Upload Failed</source>
        <translation>Échec du téléversement de la clé</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="190"/>
        <source>Failed to export the public key before uploading.
Key: %1</source>
        <translation>Échec de l&apos;exportation de la clé publique avant le téléversement.
Clé : %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="274"/>
        <source>The public key was successfully uploaded to the key server %4.
Fingerprint: %1

%2
Please check your email (%3) for further verification from %4.</source>
        <translation>La clé publique a été téléversée avec succès sur le serveur de clés %4.
Empreinte : %1

%2
Veuillez vérifier votre e-mail (%3) pour la vérification supplémentaire de %4.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="218"/>
        <location filename="../KeyServerSyncModule.cpp" line="290"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>Échec du téléversement de la clé publique vers le serveur.
Empreinte : %1
Erreur : %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="101"/>
        <source>The key server did not return a key.</source>
        <translation>Le serveur de clés n&apos;a pas retourné de clé.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="127"/>
        <source>Publish Without Verification?</source>
        <translation>Publier sans vérification ?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="129"/>
        <source>%1 does not support verified publishing (VKS), so the key would be uploaded over HKP instead.

The server will not confirm your email address, and the upload cannot be undone — HKP key servers do not let keys be removed.

Publish to %1 anyway?</source>
        <translation>%1 ne prend pas en charge la publication vérifiée (VKS), la clé serait donc téléversée via HKP à la place.

Le serveur ne confirmera pas votre adresse e-mail, et le téléversement ne peut pas être annulé — les serveurs de clés HKP ne permettent pas de supprimer les clés.

Publier sur %1 quand même ?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="232"/>
        <source>The public key was uploaded to the key server %2 over HKP.
Fingerprint: %1</source>
        <translation>La clé publique a été téléversée sur le serveur de clés %2 via HKP.
Empreinte : %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="325"/>
        <source>Key Update Failed</source>
        <translation>Échec de la mise à jour de la clé</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="326"/>
        <source>Failed to retrieve public key from %3.
Key ID: %1
Error: %2</source>
        <translation>Échec de la récupération de la clé publique depuis %3.
Identifiant de clé : %1
Erreur : %2</translation>
    </message>
    <message>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation type="vanished">Échec de la récupération de la clé publique depuis le serveur.
Identifiant de clé : %1
Erreur : %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="375"/>
        <source>Key Server</source>
        <translation>Serveur de clés</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="376"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>Importer des clés publiques depuis un serveur de clés de confiance.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="424"/>
        <source>Key Server Operations</source>
        <translation>Opérations du serveur de clés</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="429"/>
        <source>Publish Public Key to Key Server</source>
        <translation>Publier la clé publique sur le serveur de clés</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="436"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>Actualiser la clé publique depuis le serveur de clés</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="247"/>
        <source>The server could not be reached.</source>
        <translation>Le serveur est inaccessible.</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="249"/>
        <source>The server responded, but not as a key server: it supports neither the HKP nor the VKS interface.</source>
        <translation>Le serveur a répondu, mais pas en tant que serveur de clés : il ne prend en charge ni l&apos;interface HKP ni l&apos;interface VKS.</translation>
    </message>
</context>
<context>
    <name>KeyServerSettingsPage</name>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="17"/>
        <location filename="../KeyServerSettingsPage.cpp" line="58"/>
        <source>Key Server List</source>
        <translation>Liste des serveurs de clés</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="49"/>
        <location filename="../KeyServerSettingsPage.cpp" line="59"/>
        <source>Add a Key Server</source>
        <translation>Ajouter un serveur de clés</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="82"/>
        <location filename="../KeyServerSettingsPage.cpp" line="60"/>
        <source>Operations</source>
        <translation>Opérations</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="60"/>
        <location filename="../KeyServerSettingsPage.cpp" line="62"/>
        <source>Add</source>
        <translation>Ajouter</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="88"/>
        <location filename="../KeyServerSettingsPage.cpp" line="63"/>
        <source>Set As Default</source>
        <translation>Définir par défaut</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="95"/>
        <location filename="../KeyServerSettingsPage.cpp" line="64"/>
        <source>Test Selected</source>
        <translation>Tester la sélection</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="102"/>
        <location filename="../KeyServerSettingsPage.cpp" line="65"/>
        <source>Delete Selected</source>
        <translation>Supprimer la sélection</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="67"/>
        <source>https://keys.example.org</source>
        <translation>https://keys.example.org</translation>
    </message>
    <message>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP; publishing and refreshing use VKS.</source>
        <translation type="vanished">Un nouveau serveur de clés est testé sur les interfaces HKP et VKS avant d&apos;être ajouté. La recherche utilise HKP ; la publication et l&apos;actualisation utilisent VKS.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="68"/>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP. Publishing and refreshing always use the default server: over VKS where it offers it, over HKP otherwise.</source>
        <translation>Un nouveau serveur de clés est testé sur les interfaces HKP et VKS avant d&apos;être ajouté. La recherche utilise HKP. La publication et l&apos;actualisation utilisent toujours le serveur par défaut : sur VKS quand celui-ci le propose, sinon sur HKP.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Default</source>
        <translation>Par défaut</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Address</source>
        <translation>Adresse</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>HKP</source>
        <translation>HKP</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>VKS</source>
        <translation>VKS</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Status</source>
        <translation>Statut</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="76"/>
        <source>Last Tested</source>
        <translation>Dernier test</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="108"/>
        <source>yes</source>
        <translation>oui</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="109"/>
        <source>no</source>
        <translation>non</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Verified</source>
        <translation>Vérifié</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Unverified</source>
        <translation>Non vérifié</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="136"/>
        <source>never</source>
        <translation>jamais</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="162"/>
        <source>Invalid Address</source>
        <translation>Adresse invalide</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="163"/>
        <source>&quot;%1&quot; is not a valid key server address.</source>
        <translation>«%1» n&apos;est pas une adresse de serveur de clés valide.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="173"/>
        <source>Already Listed</source>
        <translation>Déjà listé</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="174"/>
        <source>%1 is already in the key server list.</source>
        <translation>%1 est déjà dans la liste des serveurs de clés.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="180"/>
        <source>Insecure Key Server Address</source>
        <translation>Adresse de serveur de clés non sécurisée</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="181"/>
        <source>%1 uses plain HTTP, so anyone on the network can see and change what you look up or publish. Add it anyway?</source>
        <translation>%1 utilise HTTP en clair, donc n&apos;importe qui sur le réseau peut voir et modifier ce que vous consultez ou publiez. L&apos;ajouter quand même ?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="229"/>
        <source>No Verified Publishing</source>
        <translation>Aucune publication vérifiée</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="230"/>
        <source>%1 does not support the VKS interface, so publishing and refreshing will use HKP instead.

Over HKP the server does not confirm your email address, and an uploaded key cannot be removed again.

Use %1 as the default anyway?</source>
        <translation>%1 ne prend pas en charge l&apos;interface VKS, donc la publication et l&apos;actualisation utiliseront HKP à la place.

Sur HKP, le serveur ne confirme pas votre adresse e-mail et une clé téléversée ne peut pas être supprimée ultérieurement.

Utiliser %1 comme serveur par défaut malgré tout&#xa0;?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="291"/>
        <source>Key Server Not Verified</source>
        <translation>Serveur de clés non vérifié</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="292"/>
        <source>%1 did not answer as a key server.

%2

It has been added and marked unverified; use Test Selected to try again.</source>
        <translation>%1 n&apos;a pas répondu en tant que serveur de clés.

%2

Il a été ajouté et marqué comme non vérifié ; utilisez Test sélectionné pour réessayer.</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Key ID</source>
        <translation>Identifiant de clé</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Creation Date</source>
        <translation>Date de création</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Expiration Date</source>
        <translation>Date d&apos;expiration</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Algorithm</source>
        <translation>Algorithme</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Key Size</source>
        <translation>Taille de la clé</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="112"/>
        <source>Status</source>
        <translation>Statut</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="115"/>
        <source>By Key ID</source>
        <translation>Par identifiant de clé</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="116"/>
        <source>By Email</source>
        <translation>Par e-mail</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="117"/>
        <source>By Fingerprint</source>
        <translation>Par empreinte</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="120"/>
        <source>Enter a value, then press Enter or Search</source>
        <translation>Saisissez une valeur, puis appuyez sur Entrée ou Rechercher</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="163"/>
        <source>Search value is empty.</source>
        <translation>La valeur de recherche est vide.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Key server URL is empty.</source>
        <translation>L&apos;URL du serveur de clés est vide.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="188"/>
        <source>Invalid key server URL format.</source>
        <translation>Format d&apos;URL du serveur de clés non valide.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="201"/>
        <source>Invalid email format.</source>
        <translation>Format d&apos;e-mail non valide.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="216"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16, 40 or 64.</source>
        <translation>Format d&apos;empreinte non valide. Il doit s&apos;agir d&apos;une chaîne hexadécimale de 16, 40 ou 64 caractères.</translation>
    </message>
    <message>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation type="vanished">Format d&apos;empreinte non valide. Il doit s&apos;agir d&apos;une chaîne hexadécimale de 16 ou 40 caractères.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="230"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>Format d&apos;identifiant de clé non valide. Il doit s&apos;agir d&apos;une chaîne hexadécimale de 8 ou 16 caractères.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="236"/>
        <source>Unknown search type.</source>
        <translation>Type de recherche inconnu.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="273"/>
        <location filename="../SearchKeyDialog.cpp" line="283"/>
        <source>No keys found matching your search.</source>
        <translation>Aucune clé trouvée correspondant à votre recherche.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="302"/>
        <source>(no user ID published)</source>
        <translation>(aucun ID utilisateur publié)</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="306"/>
        <source>Unknown</source>
        <translation>Inconnu</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="310"/>
        <source>Never</source>
        <translation>Jamais</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="374"/>
        <source>No GPG context is available.</source>
        <translation>Aucun contexte GPG n&apos;est disponible.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="14"/>
        <source>Search Keys</source>
        <translation>Rechercher des clés</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="36"/>
        <source>Key Server</source>
        <translation>Serveur de clés</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="56"/>
        <source>Search Type</source>
        <translation>Type de recherche</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="66"/>
        <source>Search Value</source>
        <translation>Valeur de recherche</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="85"/>
        <source>Search</source>
        <translation>Rechercher</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="105"/>
        <source>Tips: double click to import the selected key.</source>
        <translation>Astuce : double-cliquez pour importer la clé sélectionnée.</translation>
    </message>
</context>
</TS>
