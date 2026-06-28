<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="es_ES">
<context>
    <name>EMailMetaDataDialog</name>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="20"/>
        <source>Message</source>
        <translation>Mensaje</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="54"/>
        <source>From</source>
        <translation>De</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="77"/>
        <source>To</source>
        <translation>Para</translation>
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
        <translation>CCO</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="153"/>
        <source>Subject</source>
        <translation>Asunto</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="175"/>
        <source>Tips: You can fill in multiple email addresses, please separate them with &quot;;&quot;, except for the &apos;From&apos; field.</source>
        <translation>Nota: Puede introducir varias direcciones de correo electrónico, sepárelas con &quot;;&quot;, excepto en el campo &apos;De&apos;.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="234"/>
        <source>Cancel</source>
        <translation>Cancelar</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="241"/>
        <source>OK</source>
        <translation>Aceptar</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="196"/>
        <source>The &apos;From&apos; field cannot be empty.</source>
        <translation>El campo &apos;De&apos; no puede estar vacío.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="198"/>
        <source>The &apos;From&apos; field must contain a valid email address.</source>
        <translation>El campo &apos;De&apos; debe contener una dirección de correo electrónico válida.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="202"/>
        <source>The &apos;To&apos; field cannot be empty.</source>
        <translation>El campo &apos;Para&apos; no puede estar vacío.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="206"/>
        <source>One or more &apos;To&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Una o más direcciones de «Para» son inválidas. Separe varias direcciones con &quot;;&quot;.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="213"/>
        <source>One or more &apos;CC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Una o más direcciones de «CC» son inválidas. Separe varias direcciones con &quot;;&quot;.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="221"/>
        <source>One or more &apos;BCC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation>Una o más direcciones de «BCC» son inválidas. Separe varias direcciones con &quot;;&quot;.</translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="227"/>
        <source>The &apos;Subject&apos; field cannot be empty.</source>
        <translation>El campo «Asunto» no puede estar vacío.</translation>
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
        <translation># Error de datos EML

Los datos EML proporcionados no cumplen con los estándares RFC 3156 y no se pueden procesar.

**Detalles:** %1

### ¿Qué son los datos EML?
EML es un formato de archivo para representar mensajes de correo electrónico, que normalmente incluye encabezados, texto del cuerpo, archivos adjuntos y metadatos. Se requieren datos EML completos y correctamente estructurados para la validación.

### Soluciones sugeridas
1. Verifique que los datos EML estén completos y coincidan con la estructura descrita en RFC 3156.
2. Consulte la documentación oficial sobre la estructura EML: %2

Después de corregir los datos EML, intente realizar la operación nuevamente.</translation>
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
        <translation># Error en la operación de correo electrónico

Se produjo un error durante la operación de correo electrónico. No se pudo completar el proceso.

**Detalles:**
- **Código de error:** %1
- **Mensaje de error:** %2

### Posibles causas
1. Los datos del correo electrónico pueden estar incompletos o corruptos.
2. La clave GPG seleccionada no tiene los permisos necesarios.
3. Problemas en el entorno o la configuración de GPG.

### Soluciones sugeridas
1. Asegúrese de que los datos del correo electrónico estén completos y sigan el formato esperado.
2. Verifique que la clave GPG tenga los permisos de acceso requeridos.
3. Compruebe el entorno y la configuración de GPG.
4. Revise los detalles del error anteriores o los registros de la aplicación para más información.

Si el problema persiste, considere buscar soporte técnico o consultar la documentación.</translation>
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
        <translation>Correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="94"/>
        <location filename="../EMailModule.cpp" line="418"/>
        <location filename="../EMailModule.cpp" line="549"/>
        <location filename="../EMailModule.cpp" line="1233"/>
        <source>To</source>
        <translation>Para</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="95"/>
        <location filename="../EMailModule.cpp" line="422"/>
        <location filename="../EMailModule.cpp" line="553"/>
        <location filename="../EMailModule.cpp" line="1237"/>
        <source>Subject</source>
        <translation>Asunto</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="96"/>
        <location filename="../EMailModule.cpp" line="425"/>
        <location filename="../EMailModule.cpp" line="556"/>
        <location filename="../EMailModule.cpp" line="1240"/>
        <source>CC</source>
        <translation>CC</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="97"/>
        <location filename="../EMailModule.cpp" line="428"/>
        <location filename="../EMailModule.cpp" line="559"/>
        <location filename="../EMailModule.cpp" line="1243"/>
        <source>BCC</source>
        <translation>BCC</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="98"/>
        <location filename="../EMailModule.cpp" line="431"/>
        <location filename="../EMailModule.cpp" line="562"/>
        <location filename="../EMailModule.cpp" line="1246"/>
        <source>Date</source>
        <translation>Fecha</translation>
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
        <translation>Hash de los datos EML firmados (SHA1)</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="108"/>
        <location filename="../EMailModule.cpp" line="444"/>
        <location filename="../EMailModule.cpp" line="1259"/>
        <source>Message Integrity Check Algorithm</source>
        <translation>Algoritmo de verificación de integridad del mensaje</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="136"/>
        <source>Encryption Recipient</source>
        <translation>Destinatario de cifrado</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="137"/>
        <source>Recipient</source>
        <translation>Destinatario</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="138"/>
        <source>Key ID</source>
        <translation>ID de clave</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="453"/>
        <source>Verify E-Mail</source>
        <translation>Verificar correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="571"/>
        <source>Decrypt E-Mail</source>
        <translation>Descifrar correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="748"/>
        <location filename="../EMailModule.cpp" line="773"/>
        <source>Sign E-Mail</source>
        <translation>Firmar correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="933"/>
        <location filename="../EMailModule.cpp" line="970"/>
        <source>Encrypt E-Mail</source>
        <translation>Cifrar correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1103"/>
        <location filename="../EMailModule.cpp" line="1150"/>
        <source>Encrypt and Sign E-Mail</source>
        <translation>Cifrar y firmar correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1268"/>
        <source>Decrypt and Verify E-Mail</source>
        <translation>Descifrar y verificar correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1316"/>
        <source>Save file</source>
        <translation>Guardar archivo</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1343"/>
        <location filename="../EMailModule.cpp" line="1388"/>
        <location filename="../EMailModule.cpp" line="1415"/>
        <source>Warning</source>
        <translation>Advertencia</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1344"/>
        <source>Cannot read file%1:
%2.</source>
        <translation>No se puede leer el archivo%1:
%2.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1389"/>
        <source>The file %1 is too large (%2 bytes) to be opened. The maximum allowed size is 1 MB.</source>
        <translation>El archivo %1 es demasiado grande (%2 bytes) para abrirse. El tamaño máximo permitido es 1 MB.</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1416"/>
        <source>Cannot read file %1:
%2.</source>
        <translation>No se puede leer el archivo %1:
%2.</translation>
    </message>
</context>
<context>
    <name>GTrC</name>
    <message>
        <location filename="../EMailModule.cpp" line="320"/>
        <source>Mail Editor</source>
        <translation>Editor de correo electrónico</translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="322"/>
        <source>Open a new text editor for email.</source>
        <translation>Abrir un nuevo editor de texto para correo electrónico.</translation>
    </message>
</context>
</TS>
