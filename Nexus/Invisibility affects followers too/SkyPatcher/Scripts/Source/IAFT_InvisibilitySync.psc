Scriptname IAFT_InvisibilitySync extends ActiveMagicEffect

Spell Property FollowerInvisibilitySpell Auto

Actor PlayerRef
Actor FollowerRef

Bool SpellApplied = False
Bool Finished = False
Int UpdatesSinceCast = 0

Event OnEffectStart(Actor akTarget, Actor akCaster)
    PlayerRef = Game.GetPlayer()
    FollowerRef = akTarget

    If FollowerRef == None || PlayerRef == None
        Dispel()
        Return
    EndIf

    If FollowerRef == PlayerRef || !FollowerRef.IsPlayerTeammate()
        Dispel()
        Return
    EndIf

    If akCaster != PlayerRef || FollowerInvisibilitySpell == None
        Dispel()
        Return
    EndIf

    RegisterForSingleUpdate(0.5)
EndEvent

Event OnUpdate()
    If Finished
        Return
    EndIf

    If PlayerRef == None || FollowerRef == None
        StopSync()
        Return
    EndIf

    If !FollowerRef.IsPlayerTeammate()
        StopSync()
        Return
    EndIf

    If PlayerRef.GetActorValue("Invisibility") <= 0.0
        StopSync()
        Return
    EndIf

    If !SpellApplied
        FollowerInvisibilitySpell.Cast(FollowerRef)
        SpellApplied = True
        UpdatesSinceCast = 0
    Else
        UpdatesSinceCast = UpdatesSinceCast + 1

        If UpdatesSinceCast >= 60
			Debug.Notification("IAFT: renovando invisibilidade")

			FollowerRef.DispelSpell(FollowerInvisibilitySpell)

			Utility.Wait(0.3)

			If !Finished && FollowerRef != None && PlayerRef != None
				If PlayerRef.GetActorValue("Invisibility") > 0.0
					FollowerInvisibilitySpell.Cast(FollowerRef)
				EndIf
			EndIf

			UpdatesSinceCast = 0
		EndIf
    EndIf

    RegisterForSingleUpdate(0.5)
EndEvent

Function Cleanup()
    If Finished
        Return
    EndIf

    Finished = True

    If SpellApplied && FollowerRef != None && FollowerInvisibilitySpell != None
        FollowerRef.DispelSpell(FollowerInvisibilitySpell)
    EndIf

    SpellApplied = False
EndFunction

Function StopSync()
    Cleanup()
    Dispel()
EndFunction

Event OnEffectFinish(Actor akTarget, Actor akCaster)
    Debug.Notification("IAFT: controlador finalizado")
    Cleanup()
EndEvent