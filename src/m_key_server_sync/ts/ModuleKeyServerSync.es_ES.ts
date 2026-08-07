<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="es_ES">
<context>
    <name>GTrC</name>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="252"/>
        <source>The following email addresses have status:
</source>
        <translation>Las siguientes direcciones de correo electrónico tienen el estado:</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="262"/>
        <source>Could not parse status information.</source>
        <translation>No se pudo analizar la información de estado.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="230"/>
        <location filename="../KeyServerSyncModule.cpp" line="273"/>
        <source>Public Key Upload Successful</source>
        <translation>Carga de clave pública exitosa</translation>
    </message>
    <message>
        <source>The public key was successfully uploaded to the key server keys.openpgp.org.
Fingerprint: %1

%2
Please check your email (%3) for further verification from keys.openpgp.org.

Note: For verification, you can find more information here: https://keys.openpgp.org/about</source>
        <translation type="vanished">La clave pública se cargó correctamente en el servidor de claves keys.openpgp.org.
Huella: %1

%2
Por favor, revise su correo electrónico (%3) para obtener la verificación adicional de keys.openpgp.org.

Nota: Para la verificación, puede encontrar más información aquí: https://keys.openpgp.org/about</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="189"/>
        <location filename="../KeyServerSyncModule.cpp" line="217"/>
        <location filename="../KeyServerSyncModule.cpp" line="289"/>
        <source>Key Upload Failed</source>
        <translation>Carga de clave fallida</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="190"/>
        <source>Failed to export the public key before uploading.
Key: %1</source>
        <translation>No se pudo exportar la clave pública antes de cargarla.
Clave: %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="274"/>
        <source>The public key was successfully uploaded to the key server %4.
Fingerprint: %1

%2
Please check your email (%3) for further verification from %4.</source>
        <translation>La clave pública se cargó correctamente en el servidor de claves %4.
Huella: %1

%2
Por favor, revise su correo electrónico (%3) para obtener la verificación adicional de %4.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="218"/>
        <location filename="../KeyServerSyncModule.cpp" line="290"/>
        <source>Failed to upload public key to the server.
Fingerprint: %1
Error: %2</source>
        <translation>No se pudo cargar la clave pública en el servidor.
Huella: %1
Error: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="101"/>
        <source>The key server did not return a key.</source>
        <translation>El servidor de claves no devolvió una clave.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="127"/>
        <source>Publish Without Verification?</source>
        <translation>¿Publicar sin verificación?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="129"/>
        <source>%1 does not support verified publishing (VKS), so the key would be uploaded over HKP instead.

The server will not confirm your email address, and the upload cannot be undone — HKP key servers do not let keys be removed.

Publish to %1 anyway?</source>
        <translation>%1 no admite la publicación verificada (VKS), por lo que la clave se cargaría a través de HKP.

El servidor no confirmará su dirección de correo electrónico y la carga no se puede deshacer: los servidores de claves HKP no permiten eliminar claves.

¿Publicar en %1 de todos modos?</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="232"/>
        <source>The public key was uploaded to the key server %2 over HKP.
Fingerprint: %1</source>
        <translation>La clave pública se cargó en el servidor de claves %2 a través de HKP.
Huella: %1</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="325"/>
        <source>Key Update Failed</source>
        <translation>Actualización de clave fallida</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="326"/>
        <source>Failed to retrieve public key from %3.
Key ID: %1
Error: %2</source>
        <translation>No se pudo recuperar la clave pública de %3.
ID de clave: %1
Error: %2</translation>
    </message>
    <message>
        <source>Failed to retrieve public key from the server.
Key ID: %1
Error: %2</source>
        <translation type="vanished">No se pudo recuperar la clave pública del servidor.
ID de clave: %1
Error: %2</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="375"/>
        <source>Key Server</source>
        <translation>Servidor de claves</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="376"/>
        <source>Import public keys from a trusted key server.</source>
        <translation>Importar claves públicas desde un servidor de claves de confianza.</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="424"/>
        <source>Key Server Operations</source>
        <translation>Operaciones del servidor de claves</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="429"/>
        <source>Publish Public Key to Key Server</source>
        <translation>Publicar clave pública en el servidor de claves</translation>
    </message>
    <message>
        <location filename="../KeyServerSyncModule.cpp" line="436"/>
        <source>Refresh Public Key From Key Server</source>
        <translation>Actualizar clave pública desde el servidor de claves</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="247"/>
        <source>The server could not be reached.</source>
        <translation>No se pudo contactar con el servidor.</translation>
    </message>
    <message>
        <location filename="../KeyServerProbe.cpp" line="249"/>
        <source>The server responded, but not as a key server: it supports neither the HKP nor the VKS interface.</source>
        <translation>El servidor respondió, pero no como un servidor de claves: no admite ni la interfaz HKP ni la VKS.</translation>
    </message>
</context>
<context>
    <name>KeyServerSettingsPage</name>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="17"/>
        <location filename="../KeyServerSettingsPage.cpp" line="58"/>
        <source>Key Server List</source>
        <translation>Lista de servidores de claves</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="49"/>
        <location filename="../KeyServerSettingsPage.cpp" line="59"/>
        <source>Add a Key Server</source>
        <translation>Añadir un servidor de claves</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="82"/>
        <location filename="../KeyServerSettingsPage.cpp" line="60"/>
        <source>Operations</source>
        <translation>Operaciones</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="60"/>
        <location filename="../KeyServerSettingsPage.cpp" line="62"/>
        <source>Add</source>
        <translation>Añadir</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="88"/>
        <location filename="../KeyServerSettingsPage.cpp" line="63"/>
        <source>Set As Default</source>
        <translation>Establecer como predeterminado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="95"/>
        <location filename="../KeyServerSettingsPage.cpp" line="64"/>
        <source>Test Selected</source>
        <translation>Probar seleccionado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.ui" line="102"/>
        <location filename="../KeyServerSettingsPage.cpp" line="65"/>
        <source>Delete Selected</source>
        <translation>Eliminar seleccionado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="67"/>
        <source>https://keys.example.org</source>
        <translation>https://keys.example.org</translation>
    </message>
    <message>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP; publishing and refreshing use VKS.</source>
        <translation type="vanished">Un nuevo servidor de claves se prueba contra las interfaces HKP y VKS antes de añadirse. La búsqueda utiliza HKP; la publicación y la actualización utilizan VKS.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="68"/>
        <source>A new key server is tested against the HKP and VKS interfaces before it is added. Searching uses HKP. Publishing and refreshing always use the default server: over VKS where it offers it, over HKP otherwise.</source>
        <translation>Un nuevo servidor de claves se prueba contra las interfaces HKP y VKS antes de añadirse. La búsqueda utiliza HKP. La publicación y la actualización siempre utilizan el servidor predeterminado: mediante VKS si lo ofrece, mediante HKP en caso contrario.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Default</source>
        <translation>Predeterminado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="75"/>
        <source>Address</source>
        <translation>Dirección</translation>
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
        <translation>Estado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="76"/>
        <source>Last Tested</source>
        <translation>Última prueba</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="108"/>
        <source>yes</source>
        <translation>Sí</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="109"/>
        <source>no</source>
        <translation>No</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Verified</source>
        <translation>Verificado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="127"/>
        <source>Unverified</source>
        <translation>No verificado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="136"/>
        <source>never</source>
        <translation>Nunca</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="162"/>
        <source>Invalid Address</source>
        <translation>Dirección no válida</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="163"/>
        <source>&quot;%1&quot; is not a valid key server address.</source>
        <translation>&quot;%1&quot; no es una dirección de servidor de claves válida.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="173"/>
        <source>Already Listed</source>
        <translation>Ya incluido</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="174"/>
        <source>%1 is already in the key server list.</source>
        <translation>%1 ya está en la lista de servidores de claves.</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="180"/>
        <source>Insecure Key Server Address</source>
        <translation>Dirección de servidor de claves insegura</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="181"/>
        <source>%1 uses plain HTTP, so anyone on the network can see and change what you look up or publish. Add it anyway?</source>
        <translation>%1 usa HTTP sin cifrar, por lo que cualquiera en la red puede ver y modificar lo que busque o publique. ¿Agregarlo de todas formas?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="229"/>
        <source>No Verified Publishing</source>
        <translation>Sin publicación verificada</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="230"/>
        <source>%1 does not support the VKS interface, so publishing and refreshing will use HKP instead.

Over HKP the server does not confirm your email address, and an uploaded key cannot be removed again.

Use %1 as the default anyway?</source>
        <translation>%1 no admite la interfaz VKS, por lo que la publicación y la actualización utilizarán HKP en su lugar.

Con HKP, el servidor no confirma su dirección de correo electrónico y una clave subida no se puede eliminar nuevamente.

¿Usar %1 como predeterminado de todos modos?</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="291"/>
        <source>Key Server Not Verified</source>
        <translation>Servidor de claves no verificado</translation>
    </message>
    <message>
        <location filename="../KeyServerSettingsPage.cpp" line="292"/>
        <source>%1 did not answer as a key server.

%2

It has been added and marked unverified; use Test Selected to try again.</source>
        <translation>%1 no respondió como servidor de claves.

%2

Se ha agregado y se ha marcado como no verificado; use Probar seleccionado para intentarlo de nuevo.</translation>
    </message>
</context>
<context>
    <name>SearchKeyDialog</name>
    <message>
        <location filename="../SearchKeyDialog.ui" line="14"/>
        <source>Search Keys</source>
        <translation>Buscar claves</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="36"/>
        <source>Key Server</source>
        <translation>Servidor de claves</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="56"/>
        <source>Search Type</source>
        <translation>Tipo de búsqueda</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="66"/>
        <source>Search Value</source>
        <translation>Valor de búsqueda</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="85"/>
        <source>Search</source>
        <translation>Buscar</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.ui" line="105"/>
        <source>Tips: double click to import the selected key.</source>
        <translation>Consejo: haga doble clic para importar la clave seleccionada.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Key ID</source>
        <translation>ID de clave</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>UID</source>
        <translation>UID</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="110"/>
        <source>Creation Date</source>
        <translation>Fecha de creación</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Expiration Date</source>
        <translation>Fecha de expiración</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Algorithm</source>
        <translation>Algoritmo</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="111"/>
        <source>Key Size</source>
        <translation>Tamaño de la clave</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="112"/>
        <source>Status</source>
        <translation>Estado</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="115"/>
        <source>By Key ID</source>
        <translation>Por ID de clave</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="116"/>
        <source>By Email</source>
        <translation>Por correo electrónico</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="117"/>
        <source>By Fingerprint</source>
        <translation>Por huella</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="120"/>
        <source>Enter a value, then press Enter or Search</source>
        <translation>Ingrese un valor, luego presione Enter o Buscar</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="163"/>
        <source>Search value is empty.</source>
        <translation>El valor de búsqueda está vacío.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="179"/>
        <source>Key server URL is empty.</source>
        <translation>La URL del servidor de claves está vacía.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="188"/>
        <source>Invalid key server URL format.</source>
        <translation>Formato de URL del servidor de claves no válido.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="201"/>
        <source>Invalid email format.</source>
        <translation>Formato de correo electrónico no válido.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="216"/>
        <source>Invalid fingerprint format. It should be a hex string of length 16, 40 or 64.</source>
        <translation>Formato de huella no válido. Debe ser una cadena hexadecimal de 16, 40 o 64 caracteres.</translation>
    </message>
    <message>
        <source>Invalid fingerprint format. It should be a hex string of length 16 or 40.</source>
        <translation type="vanished">Formato de huella no válido. Debe ser una cadena hexadecimal de 16 o 40 caracteres.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="230"/>
        <source>Invalid Key ID format. It should be a hex string of length 8 or 16.</source>
        <translation>Formato de ID de clave no válido. Debe ser una cadena hexadecimal de 8 o 16 caracteres.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="236"/>
        <source>Unknown search type.</source>
        <translation>Tipo de búsqueda desconocido.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="273"/>
        <location filename="../SearchKeyDialog.cpp" line="283"/>
        <source>No keys found matching your search.</source>
        <translation>No se encontraron claves que coincidan con su búsqueda.</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="302"/>
        <source>(no user ID published)</source>
        <translation>(sin ID de usuario publicado)</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="306"/>
        <source>Unknown</source>
        <translation>Desconocido</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="310"/>
        <source>Never</source>
        <translation>Nunca</translation>
    </message>
    <message>
        <location filename="../SearchKeyDialog.cpp" line="374"/>
        <source>No GPG context is available.</source>
        <translation>No hay contexto GPG disponible.</translation>
    </message>
</context>
</TS>
