<?xml version="1.0"?>
<?CDF version="1.2"?>
<!DOCTYPE SOFTPKG SYSTEM "c:\cdf.dtd">
<SOFTPKG NAME="{38D47F1D-07F2-4CA0-89DB-6F89598DA97C}" VERSION="5.0.0" TYPE="VISIBLE">
	<TITLE>OpenG ZIP Tools</TITLE>
	<ABSTRACT>Support files neccessary to use the OpenG ZIP library.</ABSTRACT>
	<IMPLEMENTATION>
		<OS VALUE="VxWorks-PPC603" />
		<CODEBASE FILENAME="vxworks63/lvzlib.out" TARGET="/ni-rt/system/lvzlib.out"/>
		<DEPENDENCY>
			<SOFTPKG NAME="{899452D2-C085-430B-B76D-7FDB33BB324A}" VERSION="8.5.0">
				<TITLE>LabVIEW RT</TITLE>
			</SOFTPKG>
		</DEPENDENCY>
	</IMPLEMENTATION>
</SOFTPKG>