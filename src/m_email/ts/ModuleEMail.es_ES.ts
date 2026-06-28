<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="es_ES">
<context>
    <name>EMailMetaDataDialog</name>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="20"/>
        <source>Message</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="54"/>
        <source>From</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="77"/>
        <source>To</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="100"/>
        <location filename="../EMailMetaDataDialog.ui" line="207"/>
        <source>CC</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="123"/>
        <location filename="../EMailMetaDataDialog.ui" line="214"/>
        <source>BCC</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="153"/>
        <source>Subject</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="175"/>
        <source>Tips: You can fill in multiple email addresses, please separate them with &quot;;&quot;, except for the &apos;From&apos; field.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="234"/>
        <source>Cancel</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.ui" line="241"/>
        <source>OK</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="196"/>
        <source>The &apos;From&apos; field cannot be empty.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="198"/>
        <source>The &apos;From&apos; field must contain a valid email address.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="202"/>
        <source>The &apos;To&apos; field cannot be empty.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="206"/>
        <source>One or more &apos;To&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="213"/>
        <source>One or more &apos;CC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="221"/>
        <source>One or more &apos;BCC&apos; addresses are invalid. Please separate multiple addresses with &quot;;&quot;.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailMetaDataDialog.cpp" line="227"/>
        <source>The &apos;Subject&apos; field cannot be empty.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>EMailModule</name>
    <message>
        <location filename="../EMailModule.cpp" line="109"/>
        <source># EML Data Error

The provided EML data does not conform to RFC 3156 standards and cannot be processed.

**Details:** %1

### What is EML Data?
EML is a file format for representing email messages, typically including headers, body text, attachments, and metadata. Complete and properly structured EML data is required for validation.

### Suggested Solutions
1. Verify the EML data is complete and matches the structure outlined in RFC 3156.
2. Refer to the official documentation for the EML structure: %2

After correcting the EML data, try the operation again.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="134"/>
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
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="301"/>
        <location filename="../EMailModule.cpp" line="421"/>
        <location filename="../EMailModule.cpp" line="1041"/>
        <source>From</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="304"/>
        <location filename="../EMailModule.cpp" line="424"/>
        <location filename="../EMailModule.cpp" line="1044"/>
        <source>To</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="308"/>
        <location filename="../EMailModule.cpp" line="428"/>
        <location filename="../EMailModule.cpp" line="1048"/>
        <source>Subject</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="311"/>
        <location filename="../EMailModule.cpp" line="431"/>
        <location filename="../EMailModule.cpp" line="1051"/>
        <source>CC</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="314"/>
        <location filename="../EMailModule.cpp" line="434"/>
        <location filename="../EMailModule.cpp" line="1054"/>
        <source>BCC</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="317"/>
        <location filename="../EMailModule.cpp" line="437"/>
        <location filename="../EMailModule.cpp" line="1057"/>
        <source>Date</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="325"/>
        <location filename="../EMailModule.cpp" line="1065"/>
        <source>Signed EML Data Hash (SHA1)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="330"/>
        <location filename="../EMailModule.cpp" line="1070"/>
        <source>Message Integrity Check Algorithm</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1120"/>
        <source>Save file</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1147"/>
        <location filename="../EMailModule.cpp" line="1192"/>
        <location filename="../EMailModule.cpp" line="1219"/>
        <source>Warning</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1148"/>
        <source>Cannot read file%1:
%2.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1193"/>
        <source>The file %1 is too large (%2 bytes) to be opened. The maximum allowed size is 1 MB.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="1220"/>
        <source>Cannot read file %1:
%2.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>GTrC</name>
    <message>
        <location filename="../EMailModule.cpp" line="210"/>
        <source>Mail Editor</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../EMailModule.cpp" line="212"/>
        <source>Open a new text editor for email.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
</TS>
