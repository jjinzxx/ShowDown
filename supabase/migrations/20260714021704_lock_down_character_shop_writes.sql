-- All shop mutations are server-validated RPCs. Prevent authenticated clients
-- from bypassing purchase/equip checks through direct Data API writes.
-- The filename version matches the migration recorded by the hosted project.
drop policy if exists "Users can insert own skins"
on public.player_skins;

drop policy if exists "Users can insert own equipment"
on public.player_equipment;

drop policy if exists "Users can update own equipment"
on public.player_equipment;

revoke all privileges on table
  public.skins,
  public.skin_sets,
  public.skin_set_items,
  public.player_skins,
  public.player_skin_sets,
  public.player_equipment,
  public.player_wallets
from anon;

revoke all privileges on table
  public.skins,
  public.skin_sets,
  public.skin_set_items,
  public.player_skins,
  public.player_skin_sets,
  public.player_equipment,
  public.player_wallets
from authenticated;

grant select on table
  public.skins,
  public.skin_sets,
  public.skin_set_items,
  public.player_skins,
  public.player_skin_sets,
  public.player_equipment,
  public.player_wallets
to authenticated;
