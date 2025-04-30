(section-xml)=
# Introduction to XML


- The content of an XML file is composed of elements. Each element is marked by a pair of tags ``<tag>...</tag>``.
- The hierarchical structure of elements is important. At the same level of hierarchy, the arrangement order of elements is not important.
- The tag names are keywords, which should not be changed.
- Each element can have multiple attributes, which are ``name="value"`` pairs. The names are also keywords. The values must be surrounded by double quotes.



## XML Schema

The XML schema for PreVABS input files is available at :download:`prevabs.xsd <schemas/prevabs.xsd>`. 
This schema defines the structure and constraints for:

- Cross-section definition
- Material properties
- Component configurations
- Analysis settings

You can use this schema to validate your input files using any XML validator. For example, using xmllint:

```bash
xmllint --schema prevabs.xsd your_input.xml --noout
```

The schema provides validation for:

- Required and optional elements
- Valid element types and attributes
- Allowed values for enumerations
- Proper nesting of elements
