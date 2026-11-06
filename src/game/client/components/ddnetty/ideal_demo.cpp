// ddnetty: the ideal run demo, see ideal_demo.h
//
// What a segment demo holds: the snapshots the server sent, one every 2 ticks. A tee's position
// in a snapshot belongs to its own tick (m_Tick); between updates the tee just follows the physics,
// so the real position at a tick is found by moving it on from there. Seams are matched with that.
#include "ideal_demo.h"

#include <base/log.h>
#include <base/mem.h>
#include <base/vmath.h>

#include <engine/client.h>
#include <engine/shared/snapshot.h>

#include <generated/protocol.h>

#include <game/gamecore.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace
{
	constexpr float TELEPORT_SLACK = 64.0f; // px a tee can move more than its speed says before it counts as teleported
	constexpr int SEARCH_TICKS = 60; // how far from a seam the team mate may change demos
	constexpr float DUO_JUMP = 96.0f; // px: a team mate further off than this changes place at once instead of sliding

	struct STee
	{
		bool m_Valid = false;
		CNetObj_Character m_Char{};
		bool m_HasDDNet = false;
		CNetObj_DDNetCharacter m_DDNet{};
	};

	struct SFrame
	{
		int m_Tick = 0;
		size_t m_Snapshot = 0; // index into SSource::m_vSnapshots
		STee m_aTee[2]; // 0 = you, 1 = team mate
	};

	struct SSource
	{
		std::vector<IClient::SDemoSnapshot> m_vSnapshots;
		std::vector<SFrame> m_vFrames; // sorted by tick
		int m_A = 0; // segment in this demo's ticks
		int m_B = 0;
		int m_aId[2] = {-1, -1};
		int m_aTeleport[2] = {-1, -1}; // first teleport of each tee after m_A, -1 if none close
	};

	const CSnapshot *SnapshotOf(const IClient::SDemoSnapshot &Snapshot)
	{
		return reinterpret_cast<const CSnapshot *>(Snapshot.m_vData.data());
	}

	void ReadTee(const CSnapshot *pSnap, int Id, STee &Out)
	{
		if(Id < 0)
			return;
		for(int i = 0; i < pSnap->NumItems(); i++)
		{
			const CSnapshotItem *pItem = pSnap->GetItem(i);
			if(pItem->Id() != Id)
				continue;
			const int Type = pSnap->GetItemType(i);
			const int Size = pSnap->GetItemSize(i);
			if(Type == NETOBJTYPE_CHARACTER && Size >= (int)sizeof(CNetObj_Character))
			{
				mem_copy(&Out.m_Char, pItem->Data(), sizeof(CNetObj_Character));
				Out.m_Valid = true;
			}
			else if(Type == NETOBJTYPE_DDNETCHARACTER && Size > 0)
			{
				mem_copy(&Out.m_DDNet, pItem->Data(), std::min<int>(Size, sizeof(CNetObj_DDNetCharacter)));
				Out.m_HasDDNet = true;
			}
		}
	}

	// last frame at or before Tick, -1 if none
	int FrameAt(const SSource &Src, int Tick)
	{
		auto It = std::upper_bound(Src.m_vFrames.begin(), Src.m_vFrames.end(), Tick, [](int T, const SFrame &F) { return T < F.m_Tick; });
		return (int)(It - Src.m_vFrames.begin()) - 1;
	}

	// where the tee really is at Tick: its last known state moved on by the physics
	std::optional<vec2> PosAt(const SSource &Src, int Tick, int Tee)
	{
		for(int i = std::max(FrameAt(Src, Tick), 0); i >= 0 && i < (int)Src.m_vFrames.size(); i--)
		{
			const STee &T = Src.m_vFrames[i].m_aTee[Tee];
			if(!T.m_Valid)
				continue;
			const CNetObj_Character &C = T.m_Char;
			const float N = (float)std::clamp(Tick - C.m_Tick, 0, 30);
			const float VelX = C.m_VelX / 256.0f, VelY = C.m_VelY / 256.0f;
			const float Gravity = std::fabs(VelY) < 0.01f ? 0.0f : 0.25f * N * (N + 1.0f);
			return vec2(C.m_X + VelX * N, C.m_Y + VelY * N + Gravity);
		}
		return std::nullopt;
	}

	// first tick in (From, From + Within] at which the tee jumps further than it can move
	int TeleportAfter(const SSource &Src, int From, int Tee, int Within)
	{
		const STee *pPrev = nullptr;
		int PrevTick = 0;
		for(const SFrame &F : Src.m_vFrames)
		{
			if(F.m_Tick < From - 4 || !F.m_aTee[Tee].m_Valid)
				continue;
			if(F.m_Tick > From + Within)
				break;
			const STee &T = F.m_aTee[Tee];
			if(pPrev && F.m_Tick > From)
			{
				const vec2 P0(pPrev->m_Char.m_X, pPrev->m_Char.m_Y), P1(T.m_Char.m_X, T.m_Char.m_Y);
				const float Speed = std::max(length(vec2(pPrev->m_Char.m_VelX, pPrev->m_Char.m_VelY)), length(vec2(T.m_Char.m_VelX, T.m_Char.m_VelY))) / 256.0f;
				if(distance(P0, P1) > Speed * (F.m_Tick - PrevTick) + TELEPORT_SLACK)
					return F.m_Tick;
			}
			pPrev = &T;
			PrevTick = F.m_Tick;
		}
		return -1;
	}

	bool IsShotSound(int Sound)
	{
		switch(Sound)
		{
		case SOUND_GUN_FIRE:
		case SOUND_SHOTGUN_FIRE:
		case SOUND_GRENADE_FIRE:
		case SOUND_GRENADE_EXPLODE:
		case SOUND_LASER_FIRE:
		case SOUND_LASER_BOUNCE:
		case SOUND_NINJA_FIRE:
		case SOUND_NINJA_HIT:
			return true;
		default:
			return false;
		}
	}

	int Shift(int Tick, int Delta)
	{
		return Tick > 0 ? Tick + Delta : Tick;
	}
} // namespace

CIdealDemoJob::CIdealDemoJob(FRead Read, FWrite Write, std::vector<SClip> vClips, std::string Destination, SOptions Options) :
	m_Read(std::move(Read)), m_Write(std::move(Write)), m_vClips(std::move(vClips)), m_Destination(std::move(Destination)), m_Options(Options)
{
	Abortable(true);
}

CIdealDemoJob::CIdealDemoJob(IClient *pClient, std::vector<SClip> vClips, std::string Destination, SOptions Options) :
	CIdealDemoJob(
		[pClient](const char *pPath, std::vector<IClient::SDemoSnapshot> &vOut) { return pClient->DemoReadSnapshots(pPath, vOut); },
		[pClient](const char *pDst, const char *pMap, const std::vector<IClient::SDemoSnapshot> &vSnapshots) { return pClient->DemoWriteSnapshots(pDst, pMap, vSnapshots); },
		std::move(vClips), std::move(Destination), Options)
{
}

void CIdealDemoJob::Run()
{
	Build();
}

void CIdealDemoJob::Build()
{
	const int n = (int)m_vClips.size();
	if(n == 0)
	{
		m_Error = "no segments";
		return;
	}

	// --- read every segment demo
	std::vector<SSource> vSrc(n);
	for(int k = 0; k < n; k++)
	{
		if(State() == IJob::STATE_ABORTED)
			return;
		SSource &S = vSrc[k];
		if(!m_Read(m_vClips[k].m_Demo.c_str(), S.m_vSnapshots) || S.m_vSnapshots.empty())
		{
			m_Error = "could not read " + m_vClips[k].m_Demo;
			return;
		}
		S.m_A = (int)std::lround(m_vClips[k].m_StartTick);
		S.m_B = (int)std::lround(m_vClips[k].m_EndTick);
		S.m_aId[0] = m_vClips[k].m_MainId;
		S.m_aId[1] = m_vClips[k].m_DuoId;
		for(size_t i = 0; i < S.m_vSnapshots.size(); i++)
		{
			const IClient::SDemoSnapshot &Raw = S.m_vSnapshots[i];
			if(Raw.m_vData.size() < sizeof(CSnapshot) || !SnapshotOf(Raw)->IsValid(Raw.m_vData.size()))
				continue;
			SFrame F;
			F.m_Tick = Raw.m_Tick;
			F.m_Snapshot = i;
			for(int q = 0; q < 2; q++)
				ReadTee(SnapshotOf(Raw), S.m_aId[q], F.m_aTee[q]);
			if(!S.m_vFrames.empty() && S.m_vFrames.back().m_Tick >= F.m_Tick)
				continue;
			S.m_vFrames.push_back(F);
		}
		if(S.m_vFrames.empty())
		{
			m_Error = "no tee found in " + m_vClips[k].m_Demo;
			return;
		}
		m_TotalMillis += m_vClips[k].m_Millis;
	}
	const bool Duo = vSrc[0].m_aId[1] >= 0;

	// --- a seam right before a teleporter goes to the other side of it: both demos come out of
	// the teleporter at the same spot, so the change can't be seen (the total stays the same)
	for(int k = 1; k < n; k++)
	{
		const int TeleNew = TeleportAfter(vSrc[k], vSrc[k].m_A, 0, 8);
		const int TeleOld = TeleportAfter(vSrc[k - 1], vSrc[k - 1].m_B, 0, 8);
		if(TeleNew < 0 || TeleOld < 0)
			continue;
		const int Delta = std::max(TeleNew - vSrc[k].m_A, TeleOld - vSrc[k - 1].m_B) + 2;
		if(vSrc[k - 1].m_B + Delta <= vSrc[k - 1].m_vFrames.back().m_Tick && vSrc[k].m_A + Delta < vSrc[k].m_B)
		{
			vSrc[k - 1].m_B += Delta;
			vSrc[k].m_A += Delta;
		}
	}
	for(int k = 0; k < n; k++)
		for(int q = 0; q < 2; q++)
			vSrc[k].m_aTeleport[q] = TeleportAfter(vSrc[k], vSrc[k].m_A, q, 200);

	// --- output timeline: segment k starts at Bound[k]
	std::vector<int> vBound(n + 1);
	vBound[0] = vSrc[0].m_A;
	for(int k = 0; k < n; k++)
		vBound[k + 1] = vBound[k] + (vSrc[k].m_B - vSrc[k].m_A);

	// --- seams: how far apart the tees are when the demo changes
	std::vector<vec2> vOffMain(n, vec2(0, 0)), vOffDuo(n, vec2(0, 0));
	std::vector<int> vSwitch(n, 0); // the team mate changes demos this many ticks after the seam
	for(int k = 1; k < n; k++)
	{
		const std::optional<vec2> End = PosAt(vSrc[k - 1], vSrc[k - 1].m_B, 0), Start = PosAt(vSrc[k], vSrc[k].m_A, 0);
		if(End && Start)
			vOffMain[k] = *End - *Start;
		if(!Duo)
			continue;
		// the team mate changes at the tick where it is closest in both demos
		const SSource &P = vSrc[k - 1], &C = vSrc[k];
		float Best = 1e9f;
		for(int j = -SEARCH_TICKS; j <= SEARCH_TICKS; j++)
		{
			const int TP = P.m_B + j, TC = C.m_A + j;
			if(TP < P.m_vFrames.front().m_Tick || TP > P.m_vFrames.back().m_Tick || TC < C.m_vFrames.front().m_Tick || TC > C.m_vFrames.back().m_Tick)
				continue;
			const std::optional<vec2> X = PosAt(P, TP, 1), Y = PosAt(C, TC, 1);
			if(!X || !Y)
				continue;
			const float Cost = distance(*X, *Y) + 0.064f * std::abs(j);
			if(Cost < Best)
			{
				Best = Cost;
				vSwitch[k] = j;
				vOffDuo[k] = *X - *Y;
			}
		}
	}

	// --- build the output snapshots
	const int Blend = std::max(0, m_Options.m_BlendTicks);
	std::unique_ptr<CSnapshotBuilder> pBuilder = std::make_unique<CSnapshotBuilder>();
	std::unique_ptr<CSnapshotBuffer> pBuffer = std::make_unique<CSnapshotBuffer>();
	std::vector<IClient::SDemoSnapshot> vOut;

	// who the tees are (name, skin): the last segment's, so they don't change on the way
	std::vector<unsigned char> avClientInfo[2], avDDNetPlayer[2];
	for(int q = 0; q < 2; q++)
	{
		for(int k = n - 1; k >= 0 && avClientInfo[q].empty(); k--)
		{
			const SSource &S = vSrc[k];
			for(const SFrame &F : S.m_vFrames)
			{
				const CSnapshot *pSnap = SnapshotOf(S.m_vSnapshots[F.m_Snapshot]);
				for(int i = 0; i < pSnap->NumItems(); i++)
				{
					if(pSnap->GetItem(i)->Id() != S.m_aId[q])
						continue;
					const int Type = pSnap->GetItemType(i);
					const unsigned char *pData = (const unsigned char *)pSnap->GetItem(i)->Data();
					if(Type == NETOBJTYPE_CLIENTINFO)
						avClientInfo[q].assign(pData, pData + pSnap->GetItemSize(i));
					else if(Type == NETOBJTYPE_DDNETPLAYER)
						avDDNetPlayer[q].assign(pData, pData + pSnap->GetItemSize(i));
				}
				if(!avClientInfo[q].empty())
					break;
			}
		}
	}

	// the race clock of the first segment, carried over to the whole demo
	std::optional<CNetObj_GameInfo> GameInfo0;
	{
		const SSource &S = vSrc[0];
		const int f = std::max(FrameAt(S, S.m_A), 0);
		const CSnapshot *pSnap = SnapshotOf(S.m_vSnapshots[S.m_vFrames[f].m_Snapshot]);
		for(int i = 0; i < pSnap->NumItems(); i++)
			if(pSnap->GetItemType(i) == NETOBJTYPE_GAMEINFO && pSnap->GetItemSize(i) >= (int)sizeof(CNetObj_GameInfo))
				GameInfo0 = *(const CNetObj_GameInfo *)pSnap->GetItem(i)->Data();
	}

	struct SMainOut
	{
		int m_Tick;
		CNetObj_Character m_Char;
		bool m_Frozen;
	};
	std::vector<SMainOut> vMainOut;

	struct SPrevMain
	{
		int m_Segment = -1;
		int m_HookState = HOOK_IDLE;
		int m_Jumped = 0;
		int m_JumpedTotal = 0;
	} Prev;

	for(int k = 0; k < n; k++)
	{
		const SSource &S = vSrc[k];
		const int From = k == 0 ? S.m_A - 75 : S.m_A;
		const int To = k == n - 1 ? S.m_vFrames.back().m_Tick : S.m_B - 1;
		for(const SFrame &F : S.m_vFrames)
		{
			if(F.m_Tick < From || F.m_Tick > To)
				continue;
			if(State() == IJob::STATE_ABORTED)
				return;
			const int Out = vBound[k] + (F.m_Tick - S.m_A);
			const int DeltaMain = Out - F.m_Tick;
			const CSnapshot *pSrc = SnapshotOf(S.m_vSnapshots[F.m_Snapshot]);

			// where the team mate comes from at this moment
			int DuoSeg = k, DuoTick = S.m_A + (Out - vBound[k]);
			if(Duo && k + 1 < n && vSwitch[k + 1] < 0 && Out >= vBound[k + 1] + vSwitch[k + 1])
			{
				DuoSeg = k + 1;
				DuoTick = vSrc[k + 1].m_A + (Out - vBound[k + 1]);
			}
			else if(Duo && k > 0 && vSwitch[k] > 0 && Out < vBound[k] + vSwitch[k])
			{
				DuoSeg = k - 1;
				DuoTick = vSrc[k - 1].m_B + (Out - vBound[k]);
			}
			const SSource &DS = vSrc[DuoSeg];
			const int DuoFrame = FrameAt(DS, DuoTick);
			const int DeltaDuo = Out - DuoTick;

			pBuilder->Init();
			auto &&Copy = [&](int Type, int Id, const void *pData, int Size) {
				pBuilder->NewItem(Type, Id, pData, Size);
			};

			// the world: game info (race clock of the first segment), switches, pickups; shots only if wanted
			for(int i = 0; i < pSrc->NumItems(); i++)
			{
				const int Type = pSrc->GetItemType(i);
				const CSnapshotItem *pItem = pSrc->GetItem(i);
				const int Size = pSrc->GetItemSize(i);
				if(Type == NETOBJTYPE_GAMEINFO && Size >= (int)sizeof(CNetObj_GameInfo))
				{
					CNetObj_GameInfo Info = *(const CNetObj_GameInfo *)pItem->Data();
					if(GameInfo0)
					{
						const int C0 = vBound[0] - vSrc[0].m_A;
						Info.m_RoundStartTick = Shift(GameInfo0->m_RoundStartTick, C0);
						Info.m_WarmupTimer = GameInfo0->m_WarmupTimer < 0 ? GameInfo0->m_WarmupTimer - C0 : GameInfo0->m_WarmupTimer;
					}
					Copy(Type, pItem->Id(), &Info, sizeof(Info));
				}
				else if(Type == NETOBJTYPE_GAMEINFOEX || Type == NETOBJTYPE_SWITCHSTATE || Type == NETOBJTYPE_PICKUP || Type == NETOBJTYPE_DDNETPICKUP || Type == NETOBJTYPE_ENTITYEX || Type == NETOBJTYPE_MAPBESTTIME)
				{
					Copy(Type, pItem->Id(), pItem->Data(), Size);
				}
				else if(m_Options.m_KeepShots && (Type == NETOBJTYPE_PROJECTILE || Type == NETOBJTYPE_DDNETPROJECTILE || Type == NETOBJTYPE_LASER || Type == NETOBJTYPE_DDNETLASER))
				{
					std::vector<int> vData(pItem->Data(), pItem->Data() + Size / sizeof(int));
					if(Type == NETOBJTYPE_PROJECTILE && Size >= (int)sizeof(CNetObj_Projectile))
						((CNetObj_Projectile *)vData.data())->m_StartTick += DeltaMain;
					else if(Type == NETOBJTYPE_DDNETPROJECTILE && Size >= (int)sizeof(CNetObj_DDNetProjectile))
					{
						CNetObj_DDNetProjectile *p = (CNetObj_DDNetProjectile *)vData.data();
						p->m_StartTick += DeltaMain;
						p->m_Owner = p->m_Owner == S.m_aId[0] ? 0 : (p->m_Owner == S.m_aId[1] ? 1 : -1);
					}
					else if(Type == NETOBJTYPE_LASER && Size >= (int)sizeof(CNetObj_Laser))
						((CNetObj_Laser *)vData.data())->m_StartTick += DeltaMain;
					else if(Type == NETOBJTYPE_DDNETLASER && Size >= (int)sizeof(CNetObj_DDNetLaser))
					{
						CNetObj_DDNetLaser *p = (CNetObj_DDNetLaser *)vData.data();
						p->m_StartTick += DeltaMain;
						p->m_Owner = p->m_Owner == S.m_aId[0] ? 0 : (p->m_Owner == S.m_aId[1] ? 1 : -1);
					}
					Copy(Type, 2000 + pItem->Id(), vData.data(), Size);
				}
			}

			// the two tees
			vec2 aPos[2] = {vec2(0, 0), vec2(0, 0)};
			bool aHave[2] = {false, false};
			for(int q = 0; q < (Duo ? 2 : 1); q++)
			{
				const SSource &TS = q == 0 ? S : DS;
				const int FrameIndex = q == 0 ? (int)(&F - S.m_vFrames.data()) : DuoFrame;
				if(FrameIndex < 0 || FrameIndex >= (int)TS.m_vFrames.size() || !TS.m_vFrames[FrameIndex].m_aTee[q].m_Valid)
					continue;
				const SFrame &TF = TS.m_vFrames[FrameIndex];
				const int Delta = q == 0 ? DeltaMain : DeltaDuo;
				CNetObj_Character Char = TF.m_aTee[q].m_Char;
				Char.m_Tick += Delta;
				Char.m_AttackTick = Shift(Char.m_AttackTick, Delta);
				Char.m_HookedPlayer = Char.m_HookedPlayer == TS.m_aId[0] ? 0 : (Char.m_HookedPlayer == TS.m_aId[1] && TS.m_aId[1] >= 0 ? 1 : -1);

				// smooth the seam until the tee's first teleport (which hides the rest)
				const int Seg = q == 0 ? k : DuoSeg;
				const int SeamStart = q == 0 ? vBound[Seg] : vBound[Seg] + vSwitch[Seg];
				const vec2 Off = q == 0 ? vOffMain[Seg] : vOffDuo[Seg];
				const int Window = (q == 1 && length(Off) > DUO_JUMP) ? 0 : Blend;
				const int SrcTick = q == 0 ? F.m_Tick : DuoTick;
				const bool BeforeTeleport = vSrc[Seg].m_aTeleport[q] < 0 || SrcTick < vSrc[Seg].m_aTeleport[q];
				if(Seg > 0 && Window > 0 && Out >= SeamStart && Out < SeamStart + Window && BeforeTeleport)
				{
					const float f = 1.0f - (float)(Out - SeamStart) / Window;
					const int Dx = (int)std::lround(Off.x * f), Dy = (int)std::lround(Off.y * f);
					Char.m_X += Dx;
					Char.m_Y += Dy;
					Char.m_HookX += Dx;
					Char.m_HookY += Dy;
				}
				aPos[q] = vec2(Char.m_X, Char.m_Y);
				aHave[q] = true;

				// your own jumps and wall hooks: the server never sends you those sounds, so make them
				if(q == 0)
				{
					if(Prev.m_Segment == k && Out > vBound[k])
					{
						auto &&AddSound = [&](int Sound, int X, int Y) {
							CNetEvent_SoundWorld Ev;
							Ev.m_X = X;
							Ev.m_Y = Y;
							Ev.m_SoundId = Sound;
							static int s_EventId = 0;
							Copy(NETEVENTTYPE_SOUNDWORLD, 3000 + (s_EventId++ % 1000), &Ev, sizeof(Ev));
						};
						if(Char.m_HookState == HOOK_GRABBED && Prev.m_HookState != HOOK_GRABBED && Char.m_HookedPlayer < 0)
							AddSound(SOUND_HOOK_ATTACH_GROUND, Char.m_HookX, Char.m_HookY);
						if(Prev.m_HookState == HOOK_FLYING && (Char.m_HookState == HOOK_RETRACTED || (Char.m_HookState >= HOOK_RETRACT_START && Char.m_HookState <= HOOK_RETRACT_END)))
							AddSound(SOUND_HOOK_NOATTACH, Char.m_HookX, Char.m_HookY);
						const int JumpedTotal = TF.m_aTee[q].m_HasDDNet ? TF.m_aTee[q].m_DDNet.m_JumpedTotal : 0;
						if((Char.m_Jumped & 1) && !(Prev.m_Jumped & 1))
							AddSound(JumpedTotal > Prev.m_JumpedTotal && Prev.m_JumpedTotal > 0 ? SOUND_PLAYER_AIRJUMP : SOUND_PLAYER_JUMP, Char.m_X, Char.m_Y);
					}
					Prev.m_Segment = k;
					Prev.m_HookState = Char.m_HookState;
					Prev.m_Jumped = Char.m_Jumped;
					Prev.m_JumpedTotal = TF.m_aTee[q].m_HasDDNet ? TF.m_aTee[q].m_DDNet.m_JumpedTotal : 0;
				}

				if(q == 0)
				{
					const bool Frozen = TF.m_aTee[q].m_HasDDNet && TF.m_aTee[q].m_DDNet.m_FreezeEnd != 0;
					vMainOut.push_back({Out, Char, Frozen});
				}
				Copy(NETOBJTYPE_CHARACTER, q, &Char, sizeof(Char));
				if(TF.m_aTee[q].m_HasDDNet)
				{
					CNetObj_DDNetCharacter DD = TF.m_aTee[q].m_DDNet;
					DD.m_FreezeEnd = Shift(DD.m_FreezeEnd, Delta);
					DD.m_FreezeStart = Shift(DD.m_FreezeStart, Delta);
					DD.m_NinjaActivationTick = Shift(DD.m_NinjaActivationTick, Delta);
					Copy(NETOBJTYPE_DDNETCHARACTER, q, &DD, sizeof(DD));
				}
				CNetObj_PlayerInfo Info{};
				Info.m_Local = q == 0;
				Info.m_ClientId = q;
				Info.m_Team = 0;
				Copy(NETOBJTYPE_PLAYERINFO, q, &Info, sizeof(Info));
				if(!avClientInfo[q].empty())
					Copy(NETOBJTYPE_CLIENTINFO, q, avClientInfo[q].data(), (int)avClientInfo[q].size());
				if(!avDDNetPlayer[q].empty())
					Copy(NETOBJTYPE_DDNETPLAYER, q, avDDNetPlayer[q].data(), (int)avDDNetPlayer[q].size());
			}

			// sounds and effects near the two tees (not other players on the server)
			auto &&Near = [&](int X, int Y) {
				for(int q = 0; q < 2; q++)
					if(aHave[q] && distance(aPos[q], vec2(X, Y)) < 12.0f * 32.0f)
						return true;
				return false;
			};
			for(int i = 0; i < pSrc->NumItems(); i++)
			{
				const int Type = pSrc->GetItemType(i);
				const CSnapshotItem *pItem = pSrc->GetItem(i);
				const int Size = pSrc->GetItemSize(i);
				if(Size < (int)sizeof(CNetEvent_Common))
					continue;
				const CNetEvent_Common *pCommon = (const CNetEvent_Common *)pItem->Data();
				bool Keep = false;
				if(Type == NETEVENTTYPE_SOUNDWORLD && Size >= (int)sizeof(CNetEvent_SoundWorld))
					Keep = Near(pCommon->m_X, pCommon->m_Y) && (m_Options.m_KeepShots || !IsShotSound(((const CNetEvent_SoundWorld *)pCommon)->m_SoundId));
				else if(Type == NETEVENTTYPE_HAMMERHIT || Type == NETEVENTTYPE_SPAWN || Type == NETEVENTTYPE_MAPSOUNDWORLD)
					Keep = Near(pCommon->m_X, pCommon->m_Y);
				else if(Type == NETEVENTTYPE_EXPLOSION || Type == NETEVENTTYPE_DAMAGEIND)
					Keep = m_Options.m_KeepShots && Near(pCommon->m_X, pCommon->m_Y);
				if(Keep)
					Copy(Type, 1000 + pItem->Id() % 1000, pItem->Data(), Size);
			}

			const int OutSize = pBuilder->Finish(pBuffer.get());
			if(OutSize <= 0)
				continue;
			IClient::SDemoSnapshot Snapshot;
			Snapshot.m_Tick = Out;
			Snapshot.m_vData.assign((const unsigned char *)pBuffer->m_aData, (const unsigned char *)pBuffer->m_aData + OutSize);
			vOut.push_back(std::move(Snapshot));
		}
	}

	// the ghost: your tee at every tick from the start to the finish, moved on by the physics between snapshots
	m_GhostStartTick = vBound[0];
	for(int Tick = vBound[0], i = 0; Tick <= vBound[n] && !vMainOut.empty(); Tick++)
	{
		while(i + 1 < (int)vMainOut.size() && vMainOut[i + 1].m_Tick <= Tick)
			i++;
		const CNetObj_Character &C = vMainOut[i].m_Char;
		auto &&Extrapolate = [](const CNetObj_Character &Core, int T) {
			const float N = (float)std::clamp(T - Core.m_Tick, 0, 30);
			const float VelX = Core.m_VelX / 256.0f, VelY = Core.m_VelY / 256.0f;
			const float Gravity = std::fabs(VelY) < 0.01f ? 0.0f : 0.25f * N * (N + 1.0f);
			return vec2(Core.m_X + VelX * N, Core.m_Y + VelY * N + Gravity);
		};
		vec2 Pos = Extrapolate(C, Tick);
		// the physics here knows no walls: bend the path onto the next known position (not across a teleport)
		if(i + 1 < (int)vMainOut.size() && vMainOut[i + 1].m_Tick > vMainOut[i].m_Tick)
		{
			const int T0 = vMainOut[i].m_Tick, T1 = vMainOut[i + 1].m_Tick;
			const vec2 Miss = Extrapolate(vMainOut[i + 1].m_Char, T1) - Extrapolate(C, T1);
			if(length(Miss) < TELEPORT_SLACK + 4.0f * (T1 - T0))
				Pos += Miss * ((float)(Tick - T0) / (T1 - T0));
		}
		SGhostTick G;
		G.m_X = (int)std::lround(Pos.x);
		G.m_Y = (int)std::lround(Pos.y);
		G.m_VelX = C.m_VelX;
		G.m_VelY = 0;
		G.m_Angle = C.m_Angle;
		G.m_Direction = C.m_Direction;
		G.m_Weapon = vMainOut[i].m_Frozen ? WEAPON_NINJA : C.m_Weapon;
		G.m_HookState = C.m_HookState;
		G.m_HookX = C.m_HookX;
		G.m_HookY = C.m_HookY;
		G.m_AttackTick = C.m_AttackTick;
		G.m_Tick = Tick;
		m_vGhost.push_back(G);
	}
	if(avClientInfo[0].size() >= sizeof(CNetObj_ClientInfo))
	{
		const CNetObj_ClientInfo *pInfo = (const CNetObj_ClientInfo *)avClientInfo[0].data();
		m_vGhostSkin.assign(pInfo->m_aSkin, pInfo->m_aSkin + 6);
		m_vGhostSkin.push_back(pInfo->m_UseCustomColor);
		m_vGhostSkin.push_back(pInfo->m_ColorBody);
		m_vGhostSkin.push_back(pInfo->m_ColorFeet);
	}

	if(vOut.empty())
	{
		m_Error = "nothing to write";
		return;
	}
	if(!m_Write(m_Destination.c_str(), m_vClips[0].m_Demo.c_str(), vOut))
	{
		m_Error = "could not write " + m_Destination;
		return;
	}
	m_Success = true;
}
