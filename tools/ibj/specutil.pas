(* **************************************** *)
(* *                                      * *)
(* * Copyright (c) 2023 J.D.McMullin PhD. * *)
(*   All rights reserved.                 * *)
(* *                                      * *)
(* **************************************** *)
unit specutil;
interface

  procedure initialiseSpecs();
  function checkValidSpecId( specId : integer ): boolean;
  function getSpecCount(): integer;
  function getSpecTotal(): integer;
  procedure setSpecTotal( newtotal : integer );
  function newSpec( nameId : integer ): integer;
  function getSpecNameId( specIndex : integer ): integer;
  procedure setSpecNameId( specIndex : integer; newNameId : integer );
  function getSpecUsed( specIndex : integer ): boolean;
  procedure setSpecUsed( specIndex : integer );
  function getSpecP3Index( specIndex : integer ): integer;
  procedure setSpecP3Index( specIndex : integer; newP3Index : integer );
  function getSpecIsData( specIndex : integer ): boolean;
  procedure setSpecIsData( specIndex : integer );
  function getSpecIsCode( specIndex : integer ): boolean;
  procedure setSpecIsCode( specIndex : integer );
  function getSpecIsLocal( specIndex : integer ): boolean;
  procedure setSpecIsLocal( specIndex : integer );
  function getSpecIsGlobal( specIndex : integer ): boolean;
  procedure setSpecIsGlobal( specIndex : integer );
  function findSpecIndexByNameId( nameId : integer ): integer;
  function findSpecIndexByP3Index( p3Index : integer ): integer;
  procedure remapspecs();

implementation

const
  MaxSpec = 250;

type
  tSpec =
  record
    nameId  : integer;
    p3index : integer;
    used    : boolean;
    (* isData: false => code, true => data *)
    isdata  : boolean;
    (* isLocal: false => global, true => local *)
    islocal : boolean;
  end;

  tSpecArray =
  record
    count : 0..MaxSpec;
    total : 0..MaxSpec;
    data  : array [0..MaxSpec] of tSpec;
  end;

var
  Specs : tSpecArray;

  procedure initialiseSpecs();
  begin
    specs.count := 0;
    specs.total := 0;
  end;

  function checkValidSpecId( specId : integer ): boolean;
  begin
    checkValidSpecId := (0 <= specId) and (specId <= specs.count);
  end;

  function getSpecCount(): integer;
  begin
    getSpecCount := specs.count;
  end;

  function getSpecTotal(): integer;
  begin
    getSpecTotal := specs.total;
  end;

  procedure setSpecTotal( newtotal : integer );
  begin
    specs.total := newtotal;
  end;

  function newSpec( nameId : integer ): integer;
  var
    specId : integer;
  begin
    (* check to see if a spec for this name already exists *)
    specId := findSpecIndexByNameId( nameId );
    if (specId = -1) then
    begin
      (* Oh, spec doen't exist *)
      (* so, potentially add it to the list *)
      if (specs.count < MaxSpec) then
      begin
        specs.count := specs.count + 1;

        specs.data[specs.count].nameId  := nameId;
        specs.data[specs.count].p3Index := -1;
        specs.data[specs.count].used    := false;
        (* default the spec to code *)
        specs.data[specs.count].isData  := false;
        (* default visibility to global *)
        specs.data[specs.count].isLocal := false;
        specId := specs.count;
      end
      else
      begin
        (* report overflow for implementation *)
        writeln('**** FATAL **** Too many Specs for this implementation');
        writeln('**** INFO  **** Increase MaxSpec (currently ',MaxSpec,')');
      end;
    end
    else
    begin
      (* this spec already exists, *)
      (* so return the existing spec id *)
      (* this avoids duplicated specs *)
    end;

    newSpec := specId;
  end;

  function getSpecNameId( specIndex : integer ): integer;
  begin
    getSpecNameId := specs.data[specIndex].nameId;
  end;

  procedure setSpecNameId( specIndex : integer; newNameId : integer );
  begin
    specs.data[specIndex].nameId := newNameId;
  end;

  function getSpecUsed( specIndex : integer ): boolean;
  begin
    getSpecUsed := specs.data[specIndex].used;
  end;

  procedure setSpecUsed( specIndex : integer );
  begin
    specs.data[specIndex].used := true;
  end;

  function getSpecP3Index( specIndex : integer ): integer;
  begin
    getSpecP3Index := specs.data[specIndex].p3Index;
  end;

  procedure setSpecP3Index( specIndex : integer; newP3Index : integer );
  begin
    specs.data[specIndex].p3Index := newP3Index;
  end;

  function getSpecIsData( specIndex : integer ): boolean;
  begin
    getSpecIsdata := specs.data[specIndex].isdata;
  end;

  procedure setSpecIsData( specIndex : integer );
  begin
    specs.data[specIndex].isdata := true;
  end;

  function getSpecIsCode( specIndex : integer ): boolean;
  begin
    getSpecIsCode := not specs.data[specIndex].isdata;
  end;

  procedure setSpecIsCode( specIndex : integer );
  begin
    specs.data[specIndex].isdata := false;
  end;

  function getSpecIsLocal( specIndex : integer ): boolean;
  begin
    getSpecIsLocal := specs.data[specIndex].isLocal;
  end;

  procedure setSpecIsLocal( specIndex : integer );
  begin
    specs.data[specIndex].isLocal := true;
  end;

  function getSpecIsGlobal( specIndex : integer ): boolean;
  begin
    getSpecIsGlobal := not specs.data[specIndex].isLocal;
  end;

  procedure setSpecIsGlobal( specIndex : integer );
  begin
    specs.data[specIndex].isLocal := false;
  end;

  function findSpecIndexByNameId( nameId : integer ): integer;
  var
    i, specIndex : integer;
  begin
    specIndex := -1;
    i := 1;
    while (i <= Specs.count) and (specIndex = -1) do
    begin
      if (Specs.data[i].nameId = nameId) then
      begin
        specIndex := i;
      end
      else
      begin
        i := i + 1;
      end;
    end;
    findSpecIndexByNameId := specIndex;
  end;

  function findSpecIndexByP3Index( p3Index : integer ): integer;
  var
    i, specIndex : integer;
  begin
    specIndex := -1;
    i := 1;
    while (i <= Specs.count) and (specIndex = -1) do
    begin
      if (Specs.data[i].p3Index = p3Index) then
      begin
        specIndex := i;
      end
      else
      begin
        i := i + 1;
      end;
    end;
    findSpecIndexByP3Index := specIndex;
  end;

  (* for each external spec                                             *)
  (*   (1) remove the unused specs                                      *)
  (*   (2) remap the indexes of used specs to a simple zero-based index *)
  procedure remapspecs();
  var
	i, p3 : integer;
  begin
    (* NB: although Pass2 references are 1-based, our map is 0-based *)
    p3 := 0;
    for i := 1 to getSpecCount() do
    begin
      if getSpecUsed(i) then
      begin
        setSpecP3Index( i, p3 );
        p3 := p3 + 1;
      end;
    end;
    (* update the specs count *)
    (* so we know how many we will plant *)
    setSpecTotal( p3 );
  end;

end.
