(input-component)=
# Components

```xml
<component name="" type="" depend="">...</component>
```

**Specification**

- `<component>` - Root element for the definition of each component.

  - `name` - Name of the component.
  - `type` - Type of the component. Choose one from 'laminate' and 'fill'. Default is 'laminate'.
  - `depend` - List of name of dependent components, delimited by commas.


```{toctree}
:maxdepth: 2
:caption: Subtopics

component-laminate
component-fill
component-dependency
offset
```
