-- Character cosmetic catalog used by the Unreal character-skin runtime.
-- The stable ids below must stay in sync with UShowDownCharacterSkinCatalog.
-- The filename version matches the migration recorded by the hosted project.

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

-- The pre-catalog default used an id that has no matching runtime mesh. Keep
-- those rows for referential integrity, but hide the duplicate product and
-- migrate its equipment value to the stable runtime id.
update public.skin_sets
set is_active = false
where id = 'default_character';

update public.skins
set is_active = false
where id = 'default_character';

-- Robot is the permanent default. Existing accounts receive ownership and a
-- character equipment row. Preserve any non-legacy character they selected.
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
on conflict (user_id, skin_type) do update
set
  equipped_skin_id = excluded.equipped_skin_id,
  updated_at = now()
where public.player_equipment.equipped_skin_id is null
   or public.player_equipment.equipped_skin_id = 'default_character';

-- Keep future accounts on the same catalog id. This is the existing auth user
-- bootstrap function with only the character defaults and robot-set ownership
-- updated for the new shop catalog.
create or replace function public.handle_new_user()
returns trigger
language plpgsql
security definer
set search_path = ''
as $$
begin
  insert into public.profiles (id, nickname, username)
  values (
    new.id,
    coalesce(
      new.raw_user_meta_data->>'nickname',
      'Player_' || upper(substr(replace(new.id::text, '-', ''), 1, 4))
    ),
    new.raw_user_meta_data->>'username'
  )
  on conflict (id) do nothing;

  insert into public.player_wallets (user_id, coin)
  values (new.id, 0)
  on conflict (user_id) do nothing;

  insert into public.player_ranks (user_id, score)
  values (new.id, 1000)
  on conflict (user_id) do nothing;

  insert into public.player_equipment (user_id, skin_type, equipped_skin_id)
  values
    (new.id, 'card', 'default_card'),
    (new.id, 'card_back', 'default_card_back'),
    (new.id, 'life_card', 'default_life_card'),
    (new.id, 'revolver', 'default_revolver'),
    (new.id, 'character', 'robot')
  on conflict (user_id, skin_type) do nothing;

  insert into public.player_skins (user_id, skin_id)
  values
    (new.id, 'default_card'),
    (new.id, 'default_card_back'),
    (new.id, 'default_life_card'),
    (new.id, 'default_revolver'),
    (new.id, 'robot')
  on conflict (user_id, skin_id) do nothing;

  insert into public.player_skin_sets (user_id, set_id)
  values (new.id, 'character_robot_set')
  on conflict (user_id, set_id) do nothing;

  return new;
end;
$$;

create or replace function public.equip_skin_set(p_set_id text)
returns jsonb
language plpgsql
security definer
set search_path = ''
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
    where item.set_id = p_set_id
  ) then
    raise exception 'Shop item not found' using errcode = 'P0002';
  end if;

  -- The robot set is always available. Every other set must be owned.
  if not exists (
    select 1
    from public.skin_set_items item
    where item.set_id = p_set_id
      and item.slot = 'character'
      and item.skin_id = 'robot'
  ) and not exists (
    select 1
    from public.player_skin_sets owned
    where owned.user_id = v_user_id
      and owned.set_id = p_set_id
  ) then
    raise exception 'Shop item is not owned' using errcode = '42501';
  end if;

  insert into public.player_equipment (user_id, skin_type, equipped_skin_id)
  select v_user_id, item.slot, item.skin_id
  from public.skin_set_items item
  where item.set_id = p_set_id
  on conflict (user_id, skin_type) do update
  set equipped_skin_id = excluded.equipped_skin_id;

  select coalesce(jsonb_object_agg(item.slot, item.skin_id), '{}'::jsonb)
  into v_equipped
  from public.skin_set_items item
  where item.set_id = p_set_id;

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

-- The Data API no longer auto-exposes new objects. Keep the client contract
-- explicit even though this project's older tables currently retain grants.
grant select on table
  public.skins,
  public.skin_sets,
  public.skin_set_items,
  public.player_skins,
  public.player_skin_sets,
  public.player_equipment,
  public.player_wallets
to authenticated;

-- purchase_skin_set is part of the existing shop schema. Tighten its
-- SECURITY DEFINER environment and preserve the intended API role.
alter function public.purchase_skin_set(text) set search_path = '';
revoke execute on function public.purchase_skin_set(text) from public;
revoke execute on function public.purchase_skin_set(text) from anon;
grant execute on function public.purchase_skin_set(text) to authenticated;
