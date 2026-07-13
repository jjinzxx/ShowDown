-- Character cosmetic catalog used by the Unreal character-skin runtime.
-- The stable ids below must stay in sync with UShowDownCharacterSkinCatalog.

insert into public.skins (id, name, rarity, price, is_active, type)
values
  ('robot', 'Robot', 'common', 0, true, 'character'),
  ('hoodman', 'Hoodman', 'rare', 500, true, 'character'),
  ('micu', 'Micu', 'epic', 1000, true, 'character')
on conflict (id) do update
set
  name = excluded.name,
  rarity = excluded.rarity,
  price = excluded.price,
  is_active = excluded.is_active,
  type = excluded.type;

insert into public.skin_sets (id, name, rarity, price, is_active, type)
values
  ('character_robot_set', 'Robot', 'common', 0, true, 'character'),
  ('character_hoodman_set', 'Hoodman', 'rare', 500, true, 'character'),
  ('character_micu_set', 'Micu', 'epic', 1000, true, 'character')
on conflict (id) do update
set
  name = excluded.name,
  rarity = excluded.rarity,
  price = excluded.price,
  is_active = excluded.is_active,
  type = excluded.type;

insert into public.skin_set_items (set_id, skin_id, slot)
values
  ('character_robot_set', 'robot', 'character'),
  ('character_hoodman_set', 'hoodman', 'character'),
  ('character_micu_set', 'micu', 'character')
on conflict do nothing;

-- Robot is the permanent default. Existing accounts receive ownership and a
-- character equipment row without replacing an already selected character.
insert into public.player_skin_sets (user_id, set_id)
select id, 'character_robot_set'
from auth.users
on conflict do nothing;

insert into public.player_skins (user_id, skin_id)
select id, 'robot'
from auth.users
on conflict do nothing;

insert into public.player_equipment (user_id, skin_type, equipped_skin_id)
select id, 'character', 'robot'
from auth.users
on conflict (user_id, skin_type) do nothing;

create or replace function public.equip_skin_set(p_set_id text)
returns jsonb
language plpgsql
security definer
set search_path = public, auth
as $$
declare
  v_user_id uuid := auth.uid();
  v_equipped jsonb;
begin
  if v_user_id is null then
    raise exception 'Not authenticated' using errcode = '28000';
  end if;

  if p_set_id is null or btrim(p_set_id) = '' then
    raise exception 'Shop item id is required' using errcode = '22004';
  end if;

  if not exists (
    select 1
    from public.skin_set_items item
    where item.set_id::text = p_set_id
  ) then
    raise exception 'Shop item not found' using errcode = 'P0002';
  end if;

  -- The robot set is always available. Every other set must be owned.
  if not exists (
    select 1
    from public.skin_set_items item
    where item.set_id::text = p_set_id
      and item.slot = 'character'
      and item.skin_id::text = 'robot'
  ) and not exists (
    select 1
    from public.player_skin_sets owned
    where owned.user_id = v_user_id
      and owned.set_id::text = p_set_id
  ) then
    raise exception 'Shop item is not owned' using errcode = '42501';
  end if;

  insert into public.player_equipment (user_id, skin_type, equipped_skin_id)
  select v_user_id, item.slot, item.skin_id
  from public.skin_set_items item
  where item.set_id::text = p_set_id
  on conflict (user_id, skin_type) do update
  set equipped_skin_id = excluded.equipped_skin_id;

  select coalesce(jsonb_object_agg(item.slot, item.skin_id), '{}'::jsonb)
  into v_equipped
  from public.skin_set_items item
  where item.set_id::text = p_set_id;

  return jsonb_build_object(
    'success', true,
    'set_id', p_set_id,
    'equipped', v_equipped
  );
end;
$$;

revoke execute on function public.equip_skin_set(text) from public;
revoke execute on function public.equip_skin_set(text) from anon;
grant execute on function public.equip_skin_set(text) to authenticated;
