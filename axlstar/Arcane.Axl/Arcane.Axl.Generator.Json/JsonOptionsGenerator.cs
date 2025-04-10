//-----------------------------------------------------------------------------
// Copyright 2000-2022 CEA (www.cea.fr) IFPEN (www.ifpenergiesnouvelles.com)
// See the top-level COPYRIGHT file for details.
// SPDX-License-Identifier: Apache-2.0
//-----------------------------------------------------------------------------
using System;
using System.IO;

namespace Arcane.Axl
{
  /**
   * Classe implémentant le Design Pattern du visiteur pour générer le code C++ de
   * déclaration des classes pour les options de type complex.
   */
  public class JsonOptionsGenerator : IOptionInfoVisitor
  {
    public JsonOptionsGenerator(TextWriter stream, int indent_level=0)
    {
      m_stream = stream;
      m_indent_level = indent_level;
    }
    
    protected virtual void PrintOptionProperties(Option info, bool isComplex=false)
    {
      m_stream.Write(new string(' ', m_indent_level * 4) + "\"Name\": \"" + info.Name + "\",\n");
      m_stream.Write(new string(' ', m_indent_level * 4) + "\"Type\": \"" + info.Type + "\",\n");

      m_stream.Write(new string(' ', m_indent_level * 4) + "\"HasDefault\": " + (info.HasDefault ? "true":"false") + ",\n");
      if(info.HasDefault)
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"DefaultValue\": \"" + info.DefaultValue + "\",\n");
      else
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"DefaultValue\": null,\n");

      m_stream.Write(new string(' ', m_indent_level * 4) + "\"HasMinOccursAttribute\": " + (info.HasMinOccursAttribute ? "true":"false") + ",\n");
      if(info.HasMinOccursAttribute)
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"MinOccurs\": " + info.MinOccurs + ",\n");
      else
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"MinOccurs\": null,\n");

      m_stream.Write(new string(' ', m_indent_level * 4) + "\"HasMaxOccursAttribute\": " + (info.HasMaxOccursAttribute ? "true":"false") + ",\n");
      if(info.HasMaxOccursAttribute)
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"MaxOccurs\": " + info.MaxOccurs + ",\n");
      else
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"MaxOccurs\": null,\n");

      m_stream.Write(new string(' ', m_indent_level * 4) + "\"IsOptional\": " + (info.IsOptional ? "true":"false") + ",\n");
      m_stream.Write(new string(' ', m_indent_level * 4) + "\"FullName\": \"" + info.FullName + "\",\n");
      
      if(isComplex)
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"Id\": \"" + info.GetIdString() + "\",\n");
      else
        m_stream.Write(new string(' ', m_indent_level * 4) + "\"Id\": \"" + info.GetIdString() + "\"\n");
      //m_stream.Write(new string(' ', m_indent_level * 4) + "ServiceOrModule: " + info.ServiceOrModule);
    }

    public virtual void VisitComplex(ComplexOptionInfo info)
    {
      m_stream.Write(new string(' ', m_indent_level * 4) + "{" + "\n");
      m_indent_level++;
      
      PrintOptionProperties(info, true);

      m_stream.Write(new string(' ', m_indent_level * 4) + "\"Options\": [\n");
      m_indent_level++;

      for(int i=0; i<info.Options.Count; i++) {
        info.Options[i].Accept(this);
        if(i!=info.Options.Count-1)
          m_stream.Write(",\n");
      }

      m_stream.Write(new string(' ', m_indent_level * 4) + "]\n");

      m_indent_level--;
      m_indent_level--;
      m_stream.Write(new string(' ', m_indent_level * 4) + " }" + "\n");
    }
    
    public virtual void VisitExtended(ExtendedOptionInfo info) { 
      m_stream.Write(new string(' ', m_indent_level * 4) + "{" + "\n");
      m_indent_level++;
      PrintOptionProperties(info);
      m_indent_level--;
      m_stream.Write(new string(' ', m_indent_level * 4) + " }" + "\n");
    }
    public virtual void VisitEnumeration(EnumerationOptionInfo info) {
      m_stream.Write(new string(' ', m_indent_level * 4) + "{" + "\n");
      m_indent_level++;
      PrintOptionProperties(info);
      m_indent_level--;
      m_stream.Write(new string(' ', m_indent_level * 4) + " }" + "\n");
    }
    public virtual void VisitScript(ScriptOptionInfo info) { 
      m_stream.Write(new string(' ', m_indent_level * 4) + "{" + "\n");
      m_indent_level++;
      PrintOptionProperties(info);
      m_indent_level--;
      m_stream.Write(new string(' ', m_indent_level * 4) + " }" + "\n");
    }
    public virtual void VisitSimple(SimpleOptionInfo info) {
      m_stream.Write(new string(' ', m_indent_level * 4) + "{" + "\n");
      m_indent_level++;
      PrintOptionProperties(info);
      m_indent_level--;
      m_stream.Write(new string(' ', m_indent_level * 4) + " }" + "\n");
    }
    public virtual void VisitServiceInstance(ServiceInstanceOptionInfo info) {
      m_stream.Write(new string(' ', m_indent_level * 4) + "{" + "\n");
      m_indent_level++;
      PrintOptionProperties(info);
      m_indent_level--;
      m_stream.Write(new string(' ', m_indent_level * 4) + " }" + "\n");
    }
    
    private TextWriter m_stream;
    private int m_indent_level;
  }
}